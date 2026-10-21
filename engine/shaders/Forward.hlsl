// MealKit forward shading.
// 인스턴스 데이터(위치/회전/스케일/색)를 업로드 링에서 읽어 정점 셰이더에서 변환한다.
// CPU는 행렬을 만들지 않고 48바이트 인스턴스만 쓴다. 머티리얼은 glTF PBR metallic-roughness(정점의 material 번호 →
// 업로드 링의 머티리얼 표 → bindless 텍스처). 조명: 태양 (감싼 확산 + GGX 정반사) + 반구 환경광 + 지수 안개.
// 확산은 예전처럼 하프 램버트를 제곱해 부드럽게 감싼다 (조명 모델은 "텍스처와 머티리얼" 2부에서 프로젝트 설정으로)

#include "Bindless.hlsli"

struct Instance // 48바이트 (render::InstanceData)
{
    float3 position;
    uint color;      // RGBA8 (sRGB), A는 사용 안 함
    float4 rotation; // 쿼터니언
    float3 scale;
    uint flags;      // bit0: 체커 패턴, bit1: 선택 강조, bit2: 예산 초과, bit8~9: LOD (에디터 보기 모드)
};

struct FrameConstants // 192바이트 (Renderer.cpp FrameConstants)
{
    float4x4 viewProj;
    float4 cameraPos;
    float4 sunDir;      // 태양을 향하는 방향
    float4 sunColor;
    float4 skyColor;
    float4 groundColor;
    float4 fogColor;    // a = 안개 밀도
    float4 screenTint;  // 화면 번쩍임 (rgb 선형 색, a = 세기): 연출 표의 flash
    uint4 materials;    // x = 머티리얼 표의 링 위치, y = 개수
};

static const uint kNoTexture = 0xFFFFFFFFu;
struct Material // 64바이트 (Renderer.cpp MaterialGpu)
{
    float4 baseColor;
    float3 emissive;
    float alphaCutoff;
    float metallic;
    float roughness;
    float normalScale;
    uint4 textures; // baseColor, normal, metallicRoughness, emissive
};

struct PushConstants
{
    uint ring;           // 업로드 링의 g_buffers[] 번호
    uint frameOffset;    // FrameConstants 위치
    uint instanceOffset; // 이번 드로우의 Instance 배열 위치
    uint debugView;      // render::DebugView: 0 없음, 1 와이어프레임, 2 법선, 3 LOD, 4 오버드로, 5 예산
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc : register(b0);

FrameConstants loadFrame()
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint o = pc.frameOffset;
    FrameConstants f;
    // 행 우선 행렬 (CPU 메모리의 줄 = 행렬의 행)
    f.viewProj = float4x4(loadFloat4(b, o), loadFloat4(b, o + 16), loadFloat4(b, o + 32), loadFloat4(b, o + 48));
    f.cameraPos = loadFloat4(b, o + 64);
    f.sunDir = loadFloat4(b, o + 80);
    f.sunColor = loadFloat4(b, o + 96);
    f.skyColor = loadFloat4(b, o + 112);
    f.groundColor = loadFloat4(b, o + 128);
    f.fogColor = loadFloat4(b, o + 144);
    f.screenTint = loadFloat4(b, o + 160);
    f.materials = b.Load4(o + 176);
    return f;
}

Material loadMaterial(FrameConstants frame, uint index)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint o = frame.materials.x + min(index, frame.materials.y - 1) * 64;
    Material m;
    m.baseColor = loadFloat4(b, o);
    float4 e = loadFloat4(b, o + 16);
    m.emissive = e.xyz;
    m.alphaCutoff = e.w;
    float4 p = loadFloat4(b, o + 32);
    m.metallic = p.x;
    m.roughness = p.y;
    m.normalScale = p.z;
    m.textures = b.Load4(o + 48);
    return m;
}

Instance loadInstance(uint index)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint o = pc.instanceOffset + index * 48;
    Instance i;
    i.position = loadFloat3(b, o);
    i.color = b.Load(o + 12);
    i.rotation = loadFloat4(b, o + 16);
    i.scale = loadFloat3(b, o + 32);
    i.flags = b.Load(o + 44);
    return i;
}

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR; // 정점 색 (sRGB, glTF COLOR_0)
    float2 uv : TEXCOORD;
    uint2 material : MATERIAL; // x = 머티리얼 표 번호
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float3 albedo : COLOR; // 인스턴스 색 × 정점 색 (머티리얼 기본 색은 픽셀 셰이더에서)
    float2 uv : TEXCOORD;
    nointerpolation uint flags : FLAGS;
    nointerpolation uint material : MATERIAL;
};

float3 rotateByQuat(float4 q, float3 v)
{
    float3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

float3 unpackAlbedo(uint c)
{
    float3 srgb = float3(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF) / 255.0;
    return srgb * srgb; // sRGB -> 선형 근사 (감마 2.0)
}

VSOutput VSMain(VSInput input)
{
    Instance inst = loadInstance(input.instanceId);
    FrameConstants frame = loadFrame();
    float3 world = rotateByQuat(inst.rotation, input.position * inst.scale) + inst.position;

    VSOutput o;
    o.position = mul(float4(world, 1.0), frame.viewProj);
    o.worldPos = world;
    o.normal = rotateByQuat(inst.rotation, input.normal / inst.scale);
    o.albedo = unpackAlbedo(inst.color) * (input.color.rgb * input.color.rgb);
    o.uv = input.uv;
    o.flags = inst.flags;
    o.material = input.material.x;
    return o;
}

float4 sampleTexture(uint index, float2 uv, float4 fallback)
{
    return index == kNoTexture ? fallback : g_textures[NonUniformResourceIndex(index)].Sample(g_linearWrap, uv);
}

// 접선 없이 화면 미분으로 접선 공간을 만든다 (정점에 접선을 두지 않는다, docs/DECISIONS.md 2026-10-21 "정점 형식")
float3 perturbNormal(float3 n, float3 worldPos, float2 uv, float3 mapNormal)
{
    float3 dp1 = ddx(worldPos), dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv), duv2 = ddy(uv);
    float3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    float3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = rsqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
    // 왼손 좌표계로 z를 뒤집었으므로 b가 glTF와 반대 방향이다
    return normalize(mapNormal.x * t * invmax - mapNormal.y * b * invmax + mapNormal.z * n);
}

float4 PSMain(VSOutput input) : SV_Target
{
    FrameConstants frame = loadFrame();
    Material mat = loadMaterial(frame, input.material);
    float4 base = mat.baseColor * sampleTexture(mat.textures.x, input.uv, float4(1, 1, 1, 1));
    if (mat.alphaCutoff > 0.0 && base.a < mat.alphaCutoff)
        discard;
    float3 n = normalize(input.normal);
    if (mat.textures.y != kNoTexture)
    {
        float2 xy = (sampleTexture(mat.textures.y, input.uv, float4(0.5, 0.5, 1, 1)).xy * 2.0 - 1.0) * mat.normalScale;
        float3 mapNormal = float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));
        n = perturbNormal(n, input.worldPos, input.uv, mapNormal);
    }
    float4 mr = sampleTexture(mat.textures.z, input.uv, float4(1, 1, 1, 1));
    float roughness = clamp(mat.roughness * mr.g, 0.04, 1.0);
    float metallic = saturate(mat.metallic * mr.b);
    float3 emissive = mat.emissive * sampleTexture(mat.textures.w, input.uv, float4(1, 1, 1, 1)).rgb;
    float3 albedo = input.albedo * base.rgb;

    if (input.flags & 1u)
    {
        float2 cell = floor(input.worldPos.xz * 0.25);
        float checker = fmod(abs(cell.x + cell.y), 2.0);
        albedo *= lerp(1.0, 0.85, checker);
    }

    float3 l = frame.sunDir.xyz;
    float halfLambert = dot(n, l) * 0.5 + 0.5;

    // 에디터 보기 모드 (안개·번쩍임 없이)
    if (pc.debugView == 2)
        return float4(n * 0.5 + 0.5, 1.0);
    if (pc.debugView == 4) // 더하기 섞기 (선형 색): 한 겹은 어두운 갈색, 열 겹 넘으면 밝은 주황
        return float4(0.02, 0.008, 0.002, 1.0);
    if (pc.debugView == 3 || pc.debugView == 5)
    {
        uint lod = (input.flags >> 8) & 3u;
        float3 lodColor = lod == 0 ? float3(0.2, 0.8, 0.25) : (lod == 1 ? float3(0.95, 0.8, 0.15) : float3(0.95, 0.25, 0.2));
        float3 tint = pc.debugView == 3 ? lodColor : ((input.flags & 4u) ? float3(0.95, 0.2, 0.15) : float3(0.6, 0.6, 0.6));
        return float4(tint * (0.35 + 0.65 * halfLambert), 1.0);
    }

    float3 ambient = lerp(frame.groundColor.rgb, frame.skyColor.rgb, n.y * 0.5 + 0.5);

    // GGX 정반사 (태양 하나) + 금속은 확산 없이 색이 정반사로. 환경 반사는 하늘 HDRI 항목에서
    float3 v = normalize(frame.cameraPos.xyz - input.worldPos);
    float3 h = normalize(l + v);
    float ndl = saturate(dot(n, l)), ndv = max(dot(n, v), 1e-4), ndh = saturate(dot(n, h)), vdh = saturate(dot(v, h));
    float a = roughness * roughness, a2 = a * a;
    float d = a2 / (3.14159265 * pow(ndh * ndh * (a2 - 1.0) + 1.0, 2.0));
    float k = a * 0.5;
    float vis = 1.0 / ((ndl * (1.0 - k) + k) * (ndv * (1.0 - k) + k) * 4.0);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vdh, 5.0);
    float3 specular = d * vis * fresnel * ndl;
    float3 diffuse = albedo * (1.0 - metallic);
    float3 ambientSpecular = f0 * ambient * (1.0 - roughness * 0.7); // 반사 프로브 전까지의 어림
    float3 color = diffuse * (frame.sunColor.rgb * (halfLambert * halfLambert) + ambient) + specular * frame.sunColor.rgb +
                   ambientSpecular + emissive;

    if (input.flags & 2u) // 에디터 선택 강조
        color = lerp(color, float3(1.0, 0.45, 0.05), 0.45) + float3(0.08, 0.03, 0.0);

    float dist = distance(input.worldPos, frame.cameraPos.xyz);
    float fog = exp(-dist * frame.fogColor.a);
    return float4(lerp(lerp(frame.fogColor.rgb, color, fog), frame.screenTint.rgb, frame.screenTint.a), 1.0);
}
