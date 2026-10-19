// MealKit 2D UI (mealkit/ui/). 3D 장면 다음, 백버퍼 원래 해상도, 깊이 없음.
// 인스턴스 하나 = 화면의 사각형 하나 (정점 버퍼 없이 SV_VertexID로 6정점). 모양은 픽셀 셰이더의 SDF로 그린다:
// 모서리별 둥근 사각형, 단색·선형·원형 그라디언트, 안쪽 테두리, 흐린 그림자, 회전, 이미지(RGBA 아틀라스), 글자(SDF 아틀라스).
// 색은 sRGB 8비트 그대로(감마 변환 없는 오버레이 뷰), 출력은 미리 곱한 알파.

#include "Bindless.hlsli"

struct UiQuad // 112바이트 (render::UiQuad)
{
    float4 rect;     // x, y, w, h (픽셀, 회전 전 상자)
    float4 clip;     // x0, y0, x1, y1 (픽셀, 이 밖은 그리지 않는다)
    float4 radii;    // 왼쪽 위, 오른쪽 위, 오른쪽 아래, 왼쪽 아래 (픽셀)
    float4 uv;       // 아틀라스 u0, v0, u1, v1
    float4 gradient; // 선형: 시작 xy, 끝 xy / 원형: 중심 xy, 반지름 xy (상자 0..1 좌표)
    uint color0;     // 채우기 (그라디언트 시작)
    uint color1;     // 그라디언트 끝
    uint strokeColor;
    uint flags;      // 0-3 종류(0 도형, 1 이미지, 2 글자, 3 그림자), 4-5 채우기(0 단색, 1 선형, 2 원형)
    float strokeWidth;
    float rotation;  // 라디안 (상자 가운데 기준)
    float softness;  // 그림자 흐림 (픽셀)
    float sdfScale;  // 글자: 아틀라스 값 1당 화면 픽셀
};

struct PushConstants
{
    float2 screenSize;
    uint ring;       // 업로드 링의 g_buffers[] 번호
    uint quadOffset; // UiQuad 배열 위치
    uint glyphAtlas; // g_textures[] 번호: 글자 SDF (R8)
    uint imageAtlas; // g_textures[] 번호: 이미지 (RGBA8)
    uint2 pad;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0);

UiQuad loadQuad(uint index)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint o = pc.quadOffset + index * 112;
    UiQuad q;
    q.rect = loadFloat4(b, o);
    q.clip = loadFloat4(b, o + 16);
    q.radii = loadFloat4(b, o + 32);
    q.uv = loadFloat4(b, o + 48);
    q.gradient = loadFloat4(b, o + 64);
    q.color0 = b.Load(o + 80);
    q.color1 = b.Load(o + 84);
    q.strokeColor = b.Load(o + 88);
    q.flags = b.Load(o + 92);
    q.strokeWidth = asfloat(b.Load(o + 96));
    q.rotation = asfloat(b.Load(o + 100));
    q.softness = asfloat(b.Load(o + 104));
    q.sdfScale = asfloat(b.Load(o + 108));
    return q;
}

struct VSOutput
{
    float4 position : SV_Position;
    float2 local : LOCAL; // 상자 가운데 기준 픽셀 (회전 전)
    nointerpolation uint quad : QUAD;
};

float4 unpack(uint c)
{
    return float4(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, c >> 24) / 255.0;
}

VSOutput VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    UiQuad q = loadQuad(instanceId);
    const float2 corners[6] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1), float2(0, 1) };
    float2 c = corners[vertexId];
    float margin = q.softness * 2.0 + 1.0; // 테두리 바깥 반 픽셀 + 그림자 흐림
    float2 half = q.rect.zw * 0.5;
    float2 local = lerp(-half - margin, half + margin, c);
    float s = sin(q.rotation), co = cos(q.rotation);
    float2 rotated = float2(local.x * co - local.y * s, local.x * s + local.y * co);
    float2 pixel = q.rect.xy + half + rotated;
    VSOutput o;
    o.position = float4(pixel.x / pc.screenSize.x * 2.0 - 1.0, 1.0 - pixel.y / pc.screenSize.y * 2.0, 0.0, 1.0);
    o.local = local;
    o.quad = instanceId;
    return o;
}

// 모서리별 반지름 둥근 사각형의 부호 거리 (안이 음수)
float roundBox(float2 p, float2 half, float4 r)
{
    float radius = p.x > 0.0 ? (p.y > 0.0 ? r.z : r.y) : (p.y > 0.0 ? r.w : r.x);
    radius = min(radius, min(half.x, half.y));
    float2 d = abs(p) - half + radius;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;
}

float4 PSMain(VSOutput input) : SV_Target
{
    UiQuad q = loadQuad(input.quad);
    float2 screen = input.position.xy;
    if (screen.x < q.clip.x || screen.y < q.clip.y || screen.x >= q.clip.z || screen.y >= q.clip.w) discard;

    uint kind = q.flags & 0xF;
    float2 half = q.rect.zw * 0.5;
    float2 box01 = (input.local + half) / max(q.rect.zw, 1e-3);
    float d = roundBox(input.local, half, q.radii);
    float coverage = saturate(0.5 - d);

    if (kind == 3) // 그림자
    {
        float blur = max(q.softness, 0.5);
        float a = 1.0 - smoothstep(-blur, blur, d);
        float4 c = unpack(q.color0);
        return float4(c.rgb, 1.0) * (c.a * a);
    }
    if (kind == 2) // 글자 (SDF, 0.5 = 경계)
    {
        float2 uv = lerp(q.uv.xy, q.uv.zw, box01);
        float s = g_textures[pc.glyphAtlas].SampleLevel(g_linearClamp, uv, 0).r;
        float a = saturate((s - 0.5) * q.sdfScale + 0.5);
        float4 c = unpack(q.color0);
        return float4(c.rgb, 1.0) * (c.a * a);
    }

    float4 fill;
    if (kind == 1) // 이미지 (색을 곱한다)
    {
        float2 uv = lerp(q.uv.xy, q.uv.zw, box01);
        fill = g_textures[pc.imageAtlas].SampleLevel(g_linearClamp, uv, 0) * unpack(q.color0);
    }
    else
    {
        uint paint = (q.flags >> 4) & 0x3;
        float4 c0 = unpack(q.color0);
        fill = c0;
        if (paint == 1)
        {
            float2 a = q.gradient.xy, b = q.gradient.zw;
            float2 ab = b - a;
            float t = saturate(dot(box01 - a, ab) / max(dot(ab, ab), 1e-6));
            fill = lerp(c0, unpack(q.color1), t);
        }
        else if (paint == 2)
        {
            float t = saturate(length((box01 - q.gradient.xy) / max(q.gradient.zw, 1e-4)));
            fill = lerp(c0, unpack(q.color1), t);
        }
    }

    // 안쪽 테두리: 바깥 경계에서 strokeWidth 안까지
    float inner = q.strokeWidth > 0.0 ? saturate(0.5 - (d + q.strokeWidth)) : coverage;
    float strokeCoverage = coverage - inner;
    float4 stroke = unpack(q.strokeColor);
    float3 rgb = fill.rgb * fill.a * inner + stroke.rgb * stroke.a * strokeCoverage;
    float alpha = fill.a * inner + stroke.a * strokeCoverage;
    return float4(rgb, alpha);
}
