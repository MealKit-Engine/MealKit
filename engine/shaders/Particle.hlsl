// 파티클 (render::Particle, feedback::Player가 연출 표의 effect로 띄운다). 불투명 3D 다음, 깊이 시험만 하고 쓰지 않는다.
// 인스턴스 하나 = 카메라를 보는 둥근 쿼드 (정점 버퍼 없이 SV_VertexID로 6정점). 출력은 미리 곱한 알파:
// 보통 파티클은 (색 × 알파, 알파), 더하기 파티클은 (색 × 알파, 0)이라 한 파이프라인으로 두 섞기를 같이 그린다.

#include "Bindless.hlsli"

struct Particle // 32바이트 (render::Particle)
{
    float3 position;
    float size;     // 지름 (미터)
    uint color;     // sRGB RGBA8, 곧은 알파
    float rotation; // 라디안
    uint flags;     // 1 = 더하기
    float softness; // 가장자리 흐림 0..1
};

struct PushConstants // Renderer.cpp ForwardPush와 같은 모양
{
    uint ring;
    uint frameOffset;    // FrameConstants (Forward.hlsl): viewProj 0, cameraPos 64, fogColor 160
    uint particleOffset; // Particle 배열 위치
    uint3 pad;
    uint2 pad2;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0);

Particle loadParticle(uint index)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint o = pc.particleOffset + index * 32;
    Particle p;
    p.position = loadFloat3(b, o);
    p.size = asfloat(b.Load(o + 12));
    p.color = b.Load(o + 16);
    p.rotation = asfloat(b.Load(o + 20));
    p.flags = b.Load(o + 24);
    p.softness = asfloat(b.Load(o + 28));
    return p;
}

struct VSOutput
{
    float4 position : SV_Position;
    float2 local : LOCAL; // -1..1 (쿼드 안)
    nointerpolation float4 color : COLOR; // 선형 rgb, 알파
    nointerpolation float2 style : STYLE; // x = 더하기, y = 흐림
    float fog : FOG;
};

VSOutput VSMain(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint f = pc.frameOffset;
    float4x4 viewProj = float4x4(loadFloat4(b, f), loadFloat4(b, f + 16), loadFloat4(b, f + 32), loadFloat4(b, f + 48));
    float3 cameraPos = loadFloat4(b, f + 64).xyz;
    float fogDensity = loadFloat4(b, f + 160).w;

    Particle p = loadParticle(instanceId);
    const float2 corners[6] = { float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, -1), float2(1, 1), float2(-1, 1) };
    float2 c = corners[vertexId];
    float s = sin(p.rotation), co = cos(p.rotation);
    float2 r = float2(c.x * co - c.y * s, c.x * s + c.y * co);

    // 카메라를 보는 면: 오른쪽·위 축 (바로 위·아래를 볼 때는 z축을 기준으로)
    float3 toCamera = normalize(cameraPos - p.position);
    float3 up = abs(toCamera.y) > 0.99 ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 right = normalize(cross(up, toCamera));
    up = cross(toCamera, right);
    float3 world = p.position + (right * r.x + up * r.y) * (p.size * 0.5);

    VSOutput o;
    o.position = mul(float4(world, 1.0), viewProj);
    o.local = c;
    float3 srgb = float3(p.color & 0xFF, (p.color >> 8) & 0xFF, (p.color >> 16) & 0xFF) / 255.0;
    o.color = float4(srgb * srgb, (p.color >> 24) / 255.0); // sRGB -> 선형 근사 (Forward.hlsl과 같다)
    o.style = float2(p.flags & 1, saturate(p.softness));
    o.fog = exp(-fogDensity * length(cameraPos - p.position)); // 1 = 안개 없음
    return o;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float d = length(input.local);
    if (d >= 1.0) discard;
    float a = input.color.a * (1.0 - smoothstep(1.0 - max(input.style.y, 0.02), 1.0, d)) * input.fog;
    float3 rgb = input.color.rgb * a;
    return float4(rgb, input.style.x > 0.5 ? 0.0 : a);
}
