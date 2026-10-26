// MealKit forward shading.
// 인스턴스 데이터(위치/회전/스케일/색)를 업로드 링에서 읽어 정점 셰이더에서 변환한다.
// CPU는 행렬을 만들지 않고 48바이트 인스턴스만 쓴다. 머티리얼은 glTF PBR metallic-roughness(정점의 material 번호 →
// 업로드 링의 머티리얼 표 → bindless 텍스처). 조명: 태양 + 점광원(물체마다 CPU가 고른 최대 8개, 그림자 없음)
// + 환경광(하늘 HDRI의 SH, 없으면 반구 그라디언트) + 지수 안개.
// 하늘 HDRI가 있으면 VSSky/PSSky가 3D 전에 스카이박스를 그리고, 반사는 구운 큐브(밉 = 거칠기)에서 읽는다.
// 반사 프로브가 있으면 정점 셰이더가 인스턴스 위치를 담은 영향 상자 중 중심이 가장 가까운 프로브 하나를 골라 하늘 반사 대신 그 큐브를 읽는다.
// 스타일 프리셋 (docs/CODING_MODEL.md "셰이더 코드"): 조명 모델은 LIGHTING_MODEL로 빌드 때 변형 셋(PSMain),
// 머티리얼 기능(외곽선·바람·림·디졸브)은 머티리얼 표의 비트로 셰이더 안에서 고른다. 외곽선은 VSOutline/PSOutline 패스
//   0 PBR: 감싼 확산(하프 램버트 제곱) + GGX 정반사   1 툰: 램프 텍스처 확산 + 끊어진 하이라이트   2 조명 없음: 색 그대로

#include "Bindless.hlsli"

#ifndef LIGHTING_MODEL
#define LIGHTING_MODEL 0
#endif

static const uint kMaterialOutline = 1u; // render::kMaterial* (Renderer.h)
static const uint kMaterialWind = 2u;
static const uint kMaterialRim = 4u;
static const uint kMaterialDissolve = 8u;

struct Instance // 48바이트 (render::InstanceData)
{
    float3 position;
    uint color;      // RGBA8 (sRGB), A는 사용 안 함
    float4 rotation; // 쿼터니언
    float3 scale;
    uint flags;      // bit0: 체커 패턴, bit1: 선택 강조, bit2: 예산 초과, bit8~9: LOD (에디터 보기 모드),
                     // bit16~23: 번쩍임, bit24~31: 디졸브 진행도 (render::instanceFlash/instanceDissolve)
};

struct FrameConstants // 496바이트 (Renderer.cpp FrameConstants)
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
    float4 wind;        // xy = 수평 방향 (월드 xz), z = 세기, w = 화면 시간 (초)
    uint4 style;        // x = 툰 램프 텍스처 (bindless)
    float4x4 invViewProj; // 스카이박스: 화면 → 월드
    uint4 env;          // x = 하늘 큐브, y = 반사 큐브 (g_cubes), z = 반사 밉 수, w = 있음
    float4 envParams;   // x = 세기, y = cos(회전), z = sin(회전)
    float4 sh[9];       // 환경광 SH (rgb)
    uint4 probes;       // x = 반사 프로브 표의 링 위치, y = 개수 (표 32바이트: 위치 xyz, 큐브 번호 | 밉 수 << 24, 상자 반 크기 xyz, 0)
    uint4 lights;       // x = 점광원 표의 링 위치, y = 개수 (표 32바이트: 위치 xyz, 거리, 색 rgb(× 세기), 0)
};

static const uint kNoTexture = 0xFFFFFFFFu;
struct Material // 128바이트 (Renderer.cpp MaterialGpu)
{
    float4 baseColor;
    float3 emissive;
    float alphaCutoff;
    float metallic;
    float roughness;
    float normalScale;
    uint features;  // kMaterial* 비트
    uint4 textures; // baseColor, normal, metallicRoughness, emissive
    float3 outlineColor;
    float outlineWidth;
    float3 rimColor;
    float rimStrength;
    float3 dissolveColor;
    float dissolveEdge;
    float windStrength;
};

struct PushConstants
{
    uint ring;           // 업로드 링의 g_buffers[] 번호
    uint frameOffset;    // FrameConstants 위치
    uint instanceOffset; // 이번 드로우의 Instance 배열 위치
    uint debugView;      // render::DebugView: 0 없음, 1 와이어프레임, 2 법선, 3 LOD, 4 오버드로, 5 예산, 6 머티리얼 기능
    uint lightOffset;    // 인스턴스마다 점광원 번호 8개 (바이트, 0xFF = 끝)의 링 위치
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
    f.wind = loadFloat4(b, o + 192);
    f.style = b.Load4(o + 208);
    f.invViewProj = float4x4(loadFloat4(b, o + 224), loadFloat4(b, o + 240), loadFloat4(b, o + 256), loadFloat4(b, o + 272));
    f.env = b.Load4(o + 288);
    f.envParams = loadFloat4(b, o + 304);
    [unroll] for (uint i = 0; i < 9; ++i)
        f.sh[i] = loadFloat4(b, o + 320 + i * 16);
    f.probes = b.Load4(o + 464);
    f.lights = b.Load4(o + 480);
    return f;
}

Material loadMaterial(FrameConstants frame, uint index)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint o = frame.materials.x + min(index, frame.materials.y - 1) * 128;
    Material m;
    m.baseColor = loadFloat4(b, o);
    float4 e = loadFloat4(b, o + 16);
    m.emissive = e.xyz;
    m.alphaCutoff = e.w;
    float4 p = loadFloat4(b, o + 32);
    m.metallic = p.x;
    m.roughness = p.y;
    m.normalScale = p.z;
    m.features = asuint(p.w);
    m.textures = b.Load4(o + 48);
    float4 outline = loadFloat4(b, o + 64), rim = loadFloat4(b, o + 80), dissolve = loadFloat4(b, o + 96);
    m.outlineColor = outline.xyz;
    m.outlineWidth = outline.w;
    m.rimColor = rim.xyz;
    m.rimStrength = rim.w;
    m.dissolveColor = dissolve.xyz;
    m.dissolveEdge = dissolve.w;
    m.windStrength = asfloat(b.Load(o + 112));
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
    float4 color : COLOR; // 정점 색 (sRGB, glTF COLOR_0). 알파는 바람 세기
    float2 uv : TEXCOORD;
    uint2 material : MATERIAL; // x = 머티리얼 표 번호
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 localPos : LOCALPOS; // 디졸브 무늬 (물체에 붙어 움직인다)
    float3 normal : NORMAL;
    float3 albedo : COLOR; // 인스턴스 색 × 정점 색 (머티리얼 기본 색은 픽셀 셰이더에서)
    float2 uv : TEXCOORD;
    nointerpolation uint flags : FLAGS;
    nointerpolation uint material : MATERIAL;
    nointerpolation uint probe : PROBE; // 반사 프로브 큐브 번호 | 밉 수 << 24 (0 = 하늘 반사)
    nointerpolation uint2 lights : LIGHTS; // 점광원 번호 8개 (바이트, 0xFF = 끝)
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

// 인스턴스 위치를 담은 영향 상자 중 중심이 가장 가까운 반사 프로브 (물체 하나는 프로브 하나, 섞지 않는다). 없으면 0
uint nearestProbe(FrameConstants frame, float3 position)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    uint best = 0;
    float bestDistance = 3.4e38;
    for (uint i = 0; i < frame.probes.y; ++i)
    {
        uint o = frame.probes.x + i * 32;
        float3 d = loadFloat3(b, o) - position;
        float distance = dot(d, d);
        if (all(abs(d) <= loadFloat3(b, o + 16)) && distance < bestDistance)
        {
            bestDistance = distance;
            best = b.Load(o + 12) + 1; // 0과 구분 (큐브 번호 0도 있을 수 있다)
        }
    }
    return best;
}

// 정점 변환 (본 패스와 외곽선 패스가 같이 쓴다). outline이면 법선 방향으로 부풀리고, 외곽선 없는 머티리얼의 정점은 접는다
VSOutput transformVertex(VSInput input, bool outline)
{
    Instance inst = loadInstance(input.instanceId);
    FrameConstants frame = loadFrame();
    Material mat = loadMaterial(frame, input.material.x);
    float3 world = rotateByQuat(inst.rotation, input.position * inst.scale) + inst.position;
    float3 normal = rotateByQuat(inst.rotation, input.normal / inst.scale);

    if (mat.features & kMaterialWind) // 바람: 정점 알파 = 세기 (뿌리 0, 끝 1로 칠한다). 자리마다 위상이 달라 함께 흔들리지 않는다
    {
        float phase = dot(world.xz, float2(0.37, 0.23));
        float t = frame.wind.w;
        float sway = sin(t * 1.7 + phase) * 0.7 + sin(t * 3.3 + phase * 1.9) * 0.3;
        world.xz += frame.wind.xy * (sway * input.color.a * mat.windStrength * frame.wind.z);
    }
    if (outline)
        world += normalize(normal) * mat.outlineWidth;

    VSOutput o;
    o.position = mul(float4(world, 1.0), frame.viewProj);
    if (outline && !(mat.features & kMaterialOutline))
        o.position = float4(0.0, 0.0, 0.0, 0.0); // 넓이 0 삼각형은 래스터라이저가 버린다
    o.worldPos = world;
    o.localPos = input.position;
    o.normal = normal;
    o.albedo = unpackAlbedo(inst.color) * (input.color.rgb * input.color.rgb);
    o.uv = input.uv;
    o.flags = inst.flags;
    o.material = input.material.x;
    o.probe = nearestProbe(frame, inst.position);
    o.lights = frame.lights.y != 0 ? g_buffers[pc.ring].Load2(pc.lightOffset + input.instanceId * 8) : uint2(0xFFFFFFFFu, 0xFFFFFFFFu);
    return o;
}

VSOutput VSMain(VSInput input) { return transformVertex(input, false); }
VSOutput VSOutline(VSInput input) { return transformVertex(input, true); }

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

// ---- 하늘 HDRI 환경 (asset/Environment.cpp가 굽는다)

// 월드 방향 → 환경 방향 (y축 회전)
float3 envDirection(FrameConstants f, float3 d)
{
    return float3(f.envParams.y * d.x - f.envParams.z * d.z, d.y, f.envParams.z * d.x + f.envParams.y * d.z);
}

// 실수 구면 조화 0~2차 (asset/Environment.cpp shBasis와 같다). 계수에 Lambert 합성곱이 들어 있어 결과가 환경광 색이다
float3 evalSh(FrameConstants f, float3 n)
{
    float3 c = f.sh[0].rgb * 0.282095;
    c += f.sh[1].rgb * (0.488603 * n.y) + f.sh[2].rgb * (0.488603 * n.z) + f.sh[3].rgb * (0.488603 * n.x);
    c += f.sh[4].rgb * (1.092548 * n.x * n.y) + f.sh[5].rgb * (1.092548 * n.y * n.z);
    c += f.sh[6].rgb * (0.315392 * (3.0 * n.z * n.z - 1.0)) + f.sh[7].rgb * (1.092548 * n.x * n.z);
    c += f.sh[8].rgb * (0.546274 * (n.x * n.x - n.y * n.y));
    return max(c, 0.0);
}

// 환경광: HDRI의 SH, 없으면 하늘·땅 색 반구 그라디언트
float3 ambientLight(FrameConstants f, float3 n)
{
    if (f.env.w != 0)
        return evalSh(f, envDirection(f, n)) * f.envParams.x;
    return lerp(f.groundColor.rgb, f.skyColor.rgb, n.y * 0.5 + 0.5);
}

// 반사 큐브 (밉 = 거칠기): 반사 프로브가 있으면 그 큐브 (월드 방향, 장면을 구운 밝기 그대로), 없으면 하늘 HDRI
bool hasReflection(FrameConstants f, uint probe) { return probe != 0 || f.env.w != 0; }

float3 reflection(FrameConstants f, float3 r, float roughness, uint probe)
{
    if (probe != 0)
    {
        uint cube = (probe - 1) & 0xFFFFFFu;
        float probeMips = float(max((probe - 1) >> 24, 1u) - 1);
        return g_cubes[NonUniformResourceIndex(cube)].SampleLevel(g_linearClamp, r, roughness * probeMips).rgb;
    }
    float mips = float(max(f.env.z, 1u) - 1);
    return g_cubes[NonUniformResourceIndex(f.env.y)].SampleLevel(g_linearClamp, envDirection(f, r), roughness * mips).rgb * f.envParams.x;
}

// 점광원 (그림자 없음): 거리 감쇠는 창 함수를 곱한 역제곱 (range에서 0). 확산은 Lambert, PBR은 GGX 정반사도
// diffuseColor = albedo × (1 − metallic). 조명 없음 모델은 부르지 않는다
float3 pointLights(FrameConstants f, uint2 lights, float3 worldPos, float3 n, float3 v, float3 diffuseColor, float3 f0, float roughness)
{
    ByteAddressBuffer b = g_buffers[pc.ring];
    float3 color = 0.0;
    [loop] for (uint i = 0; i < 8; ++i)
    {
        uint index = ((i < 4 ? lights.x : lights.y) >> ((i & 3u) * 8)) & 0xFFu;
        if (index == 0xFFu || index >= f.lights.y)
            break;
        uint o = f.lights.x + index * 32;
        float4 pr = loadFloat4(b, o);
        float3 radiance = loadFloat3(b, o + 16);
        float3 toLight = pr.xyz - worldPos;
        float d2 = dot(toLight, toLight);
        float window = saturate(1.0 - (d2 * d2) / (pr.w * pr.w * pr.w * pr.w));
        float atten = window * window / (d2 + 1.0);
        float3 l = toLight * rsqrt(max(d2, 1e-8));
        float ndl = saturate(dot(n, l));
#if LIGHTING_MODEL == 1
        // 툰: 확산을 램프로 (해와 같은 색 단계), 하이라이트는 없다
        float3 ramp = g_textures[NonUniformResourceIndex(f.style.x)].SampleLevel(g_linearClamp, float2(saturate(dot(n, l) * 0.5 + 0.5), 0.5), 0).rgb;
        color += diffuseColor * ramp * ramp * radiance * atten;
#else
        float3 h = normalize(l + v);
        float ndh = saturate(dot(n, h)), ndv = max(dot(n, v), 1e-4), vdh = saturate(dot(v, h));
        float a = roughness * roughness, a2 = a * a;
        float d = a2 / (3.14159265 * pow(ndh * ndh * (a2 - 1.0) + 1.0, 2.0));
        float k = a * 0.5;
        float vis = 1.0 / ((ndl * (1.0 - k) + k) * (ndv * (1.0 - k) + k) * 4.0);
        float3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vdh, 5.0);
        color += (diffuseColor + d * vis * fresnel) * radiance * (atten * ndl);
#endif
    }
    return color;
}

// 거칠기·시야각에 따른 정반사 적분 (Karis의 근사, 표 텍스처 없이)
float3 envBrdf(float3 f0, float roughness, float ndv)
{
    const float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * ndv)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

// 값 무늬 (정수 해시라 GPU마다 같은 무늬). 디졸브가 쓴다
float hash3(uint3 p)
{
    uint h = p.x * 0x8da6b343u ^ p.y * 0xd8163841u ^ p.z * 0xcb1ab31fu;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    return float(h & 0xFFFFFFu) / 16777215.0;
}

float valueNoise(float3 p)
{
    float3 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    uint3 c = uint3(int3(i) + 0x4000);
    float x00 = lerp(hash3(c), hash3(c + uint3(1, 0, 0)), f.x);
    float x10 = lerp(hash3(c + uint3(0, 1, 0)), hash3(c + uint3(1, 1, 0)), f.x);
    float x01 = lerp(hash3(c + uint3(0, 0, 1)), hash3(c + uint3(1, 0, 1)), f.x);
    float x11 = lerp(hash3(c + uint3(0, 1, 1)), hash3(c + uint3(1, 1, 1)), f.x);
    return lerp(lerp(x00, x10, f.y), lerp(x01, x11, f.y), f.z);
}

// 디졸브: 무늬가 진행도보다 낮은 픽셀은 버리고, 남은 경계 폭 안은 0..1(발광 세기)을 돌려준다. 진행도 1이면 모두 사라진다
float dissolveEdge(Material mat, VSOutput input)
{
    float progress = float((input.flags >> 24) & 0xFFu) / 255.0;
    if (!(mat.features & kMaterialDissolve) || progress <= 0.0)
        return 0.0;
    float n = valueNoise(input.localPos * 6.0) * 0.7 + valueNoise(input.localPos * 17.0) * 0.3;
    float cut = progress * (1.0 + mat.dissolveEdge);
    if (n < cut)
        discard;
    return 1.0 - saturate((n - cut) / max(mat.dissolveEdge, 1e-4));
}

// 인스턴스 번쩍임 + 안개 + 화면 번쩍임 (본 패스와 외곽선이 같이)
float4 finishColor(FrameConstants frame, VSOutput input, float3 color)
{
    float flash = float((input.flags >> 16) & 0xFFu) / 255.0;
    color = lerp(color, float3(1.0, 1.0, 1.0), flash);
    float dist = distance(input.worldPos, frame.cameraPos.xyz);
    float fog = exp(-dist * frame.fogColor.a);
    return float4(lerp(lerp(frame.fogColor.rgb, color, fog), frame.screenTint.rgb, frame.screenTint.a), 1.0);
}

float4 PSMain(VSOutput input) : SV_Target
{
    FrameConstants frame = loadFrame();
    Material mat = loadMaterial(frame, input.material);
    float4 base = mat.baseColor * sampleTexture(mat.textures.x, input.uv, float4(1, 1, 1, 1));
    if (mat.alphaCutoff > 0.0 && base.a < mat.alphaCutoff)
        discard;
    float edge = dissolveEdge(mat, input);
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
    if (pc.debugView == 6) // 머티리얼 기능: 외곽선 빨강, 바람 초록, 림 파랑, 디졸브 노랑 (섞이면 더한 색)
    {
        uint f = mat.features;
        float3 tint = float3((f & kMaterialOutline) ? 0.9 : 0.0, (f & kMaterialWind) ? 0.8 : 0.0, (f & kMaterialRim) ? 0.9 : 0.0);
        if (f & kMaterialDissolve)
            tint += float3(0.8, 0.7, 0.0);
        if (f == 0)
            tint = float3(0.45, 0.45, 0.45);
        return float4(saturate(tint) * (0.35 + 0.65 * halfLambert), 1.0);
    }

    float3 ambient = ambientLight(frame, n);
    float3 v = normalize(frame.cameraPos.xyz - input.worldPos);
    float ndv = max(dot(n, v), 1e-4);

#if LIGHTING_MODEL == 0
    // GGX 정반사 (태양 하나) + 금속은 확산 없이 색이 정반사로. 환경 반사는 하늘 HDRI 항목에서
    float3 h = normalize(l + v);
    float ndl = saturate(dot(n, l)), ndh = saturate(dot(n, h)), vdh = saturate(dot(v, h));
    float a = roughness * roughness, a2 = a * a;
    float d = a2 / (3.14159265 * pow(ndh * ndh * (a2 - 1.0) + 1.0, 2.0));
    float k = a * 0.5;
    float vis = 1.0 / ((ndl * (1.0 - k) + k) * (ndv * (1.0 - k) + k) * 4.0);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vdh, 5.0);
    float3 specular = d * vis * fresnel * ndl;
    float3 diffuse = albedo * (1.0 - metallic);
    float3 ambientSpecular = hasReflection(frame, input.probe) ? reflection(frame, reflect(-v, n), roughness, input.probe) * envBrdf(f0, roughness, ndv)
                                              : f0 * ambient * (1.0 - roughness * 0.7); // HDRI가 없으면 예전 어림
    float3 color = diffuse * (frame.sunColor.rgb * (halfLambert * halfLambert) + ambient) + specular * frame.sunColor.rgb +
                   ambientSpecular + emissive + pointLights(frame, input.lights, input.worldPos, n, v, diffuse, f0, roughness);
#elif LIGHTING_MODEL == 1
    // 툰: 확산은 램프 텍스처(하프 램버트 → 색 단계), 하이라이트는 가장자리가 끊어진 점 하나. 금속은 하이라이트·환경에 색이 실린다
    float3 ramp = g_textures[NonUniformResourceIndex(frame.style.x)].SampleLevel(g_linearClamp, float2(saturate(halfLambert), 0.5), 0).rgb;
    ramp *= ramp; // sRGB 그림 → 선형 근사
    float3 h = normalize(l + v);
    float shininess = 2.0 / max(roughness * roughness * roughness * roughness, 1e-3) - 2.0;
    float highlight = smoothstep(0.45, 0.55, pow(saturate(dot(n, h)), shininess)) * step(0.0, dot(n, l)) * (1.0 - roughness * 0.8);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 diffuse = albedo * (1.0 - metallic);
    float3 envSpecular = hasReflection(frame, input.probe) ? reflection(frame, reflect(-v, n), roughness, input.probe) * envBrdf(f0, roughness, ndv)
                                          : f0 * ambient * (1.0 - roughness * 0.7);
    float3 color = diffuse * (frame.sunColor.rgb * ramp + ambient) + highlight * f0 * frame.sunColor.rgb * 4.0 + envSpecular + emissive +
                   pointLights(frame, input.lights, input.worldPos, n, v, diffuse, f0, roughness);
#else
    // 조명 없음: 색 그대로 (림·안개·번쩍임은 받는다)
    float3 color = albedo + emissive;
#endif

    if (mat.features & kMaterialRim) // 림: 보는 방향과 거의 직각인 가장자리
        color += mat.rimColor * (mat.rimStrength * pow(1.0 - ndv, 3.0));
    color += mat.dissolveColor * edge;

    if (input.flags & 2u) // 에디터 선택 강조
        color = lerp(color, float3(1.0, 0.45, 0.05), 0.45) + float3(0.08, 0.03, 0.0);

    return finishColor(frame, input, color);
}

// 외곽선 (뒤집은 껍질): 앞면을 버리고 부푼 뒷면만 단색으로. 알파 마스크·디졸브는 본 패스와 같게 잘린다
float4 PSOutline(VSOutput input) : SV_Target
{
    FrameConstants frame = loadFrame();
    Material mat = loadMaterial(frame, input.material);
    float4 base = mat.baseColor * sampleTexture(mat.textures.x, input.uv, float4(1, 1, 1, 1));
    if (mat.alphaCutoff > 0.0 && base.a < mat.alphaCutoff)
        discard;
    float edge = dissolveEdge(mat, input);
    return finishColor(frame, input, mat.outlineColor + mat.dissolveColor * edge);
}

// 스카이박스: 화면 가득 삼각형 하나 (정점 버퍼 없이). 깊이 없이 3D 전에 그리고 3D가 덮는다. 안개는 받지 않는다
struct SkyOutput
{
    float4 position : SV_Position;
    float2 clip : CLIP;
};

SkyOutput VSSky(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    SkyOutput o;
    o.clip = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    o.position = float4(o.clip, 0.0, 1.0);
    return o;
}

float4 PSSky(SkyOutput input) : SV_Target
{
    FrameConstants frame = loadFrame();
    float4 world = mul(float4(input.clip, 1.0, 1.0), frame.invViewProj); // 가까운 면 (reversed-Z)
    float3 d = normalize(world.xyz / world.w - frame.cameraPos.xyz);
    float3 color = g_cubes[NonUniformResourceIndex(frame.env.x)].SampleLevel(g_linearClamp, envDirection(frame, d), 0).rgb * frame.envParams.x;
    return float4(lerp(color, frame.screenTint.rgb, frame.screenTint.a), 1.0);
}
