// 포스트 프로세싱 (docs/DECISIONS.md 2026-10-27 "포스트 프로세싱"): 장면은 HDR 대상(R11G11B10F, 선형)에 그리고
// 블룸만 절반 해상도 듀얼 필터 사슬(PSBloomDown 단계마다 반씩 → PSBloomUp 더하기 섞기로 되돌아온다), 나머지는 우버 패스 하나(PSUber)가
// 백버퍼(또는 Ldr 대상)에 sRGB 값으로 낸다: FXAA → 색수차 → 노출 + 블룸 → AgX 톤 매핑 → 색 보정 LUT → 비네트 → 그레인.
// 내장 그래픽은 대역폭이 병목이라 효과마다 대상을 오가지 않는다. 화면 가득 삼각형 하나(정점 버퍼 없음).
//
// 읽는 좌표: 장면은 뷰포트(에디터 씬 뷰)의 픽셀 그대로 HDR 대상에 있다. 셰이더는 SV_Position(대상 픽셀)으로 읽고,
// 영역 밖(지우지 않은 곳)은 rect로 잘라 번지지 않게 한다.

#include "Bindless.hlsli"

struct PostPush // 48바이트 (render::Renderer PostPush)
{
    uint ring;        // 업로드 링 (g_buffers 번호)
    uint constOffset; // PostConstants 링 위치 (PSUber)
    uint source;      // 읽는 텍스처 (g_textures 번호): 아래 단계 / 장면
    uint first;       // PSBloomDown: 1 = 장면에서 처음 (문턱을 넘은 빛만)
    float4 rect;      // 읽는 텍스처의 유효 영역 uv (min xy, max xy)
    float2 texel;     // 읽는 텍스처의 텍셀 크기 (1 / 크기)
    float2 scale;     // 그리는 대상 픽셀 → 읽는 텍스처 uv (1 / 대상 크기, 크기가 다른 사슬 단계 사이)
};
[[vk::push_constant]] ConstantBuffer<PostPush> g_push : register(b0);

// 업로드 링의 PostConstants (render::Renderer와 같은 배치, 96바이트)
struct PostConstants
{
    float4 exposure;  // 노출 배율 (2^EV × 밝기 설정), 블룸 세기, 비네트 세기, 비네트 부드러움
    float4 effects;   // 그레인 세기, 색수차 세기 (화면 너비 비율), 그레인 시간, 0
    uint4 textures;   // 블룸 사슬 맨 위 (0xFFFFFFFF = 블룸 없음), LUT (32³ 띠, 0xFFFFFFFF = 없음), 플래그, 0
    float4 bloom;     // 블룸 색 (rgb, 선형 곱), 문턱
    float4 region;    // 장면 영역 uv (min xy, max xy): 비네트·색수차 중심
    float4 sceneSize; // 장면 대상 크기 (픽셀), 1 / 크기
};

static const uint kPostToneMap = 1u;   // 끄면 선형 값을 자르기만 (반사 프로브 굽기·보기 모드)
static const uint kPostFxaa = 2u;
static const uint kPostDither = 4u;    // 8비트 띠 줄이기 (톤 매핑할 때)
static const uint kNoTex = 0xFFFFFFFFu;

PostConstants loadPost()
{
    ByteAddressBuffer b = g_buffers[g_push.ring];
    uint o = g_push.constOffset;
    PostConstants c;
    c.exposure = loadFloat4(b, o);
    c.effects = loadFloat4(b, o + 16);
    c.textures = b.Load4(o + 32);
    c.bloom = loadFloat4(b, o + 48);
    c.region = loadFloat4(b, o + 64);
    c.sceneSize = loadFloat4(b, o + 80);
    return c;
}

struct PostVSOut
{
    float4 position : SV_Position;
};

PostVSOut VSMain(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    PostVSOut o;
    o.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float3 sampleSource(float2 uv)
{
    uv = clamp(uv, g_push.rect.xy + g_push.texel * 0.5, g_push.rect.zw - g_push.texel * 0.5);
    return g_textures[NonUniformResourceIndex(g_push.source)].SampleLevel(g_linearClamp, uv, 0).rgb;
}

float luminance(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

// ---------------------------------------------------------------- 블룸 (듀얼 필터, Marius Bjørge 2015)

float4 PSBloomDown(PostVSOut input) : SV_Target
{
    float2 uv = input.position.xy * g_push.scale; // 대상 픽셀 가운데 = 읽는 텍스처의 2×2 가운데
    float2 h = g_push.texel; // 읽는 쪽 텍셀 하나 (대각 반 텍셀 넷 + 가운데)
    float3 c = sampleSource(uv) * 4.0;
    c += sampleSource(uv + float2(-h.x, -h.y));
    c += sampleSource(uv + float2(h.x, -h.y));
    c += sampleSource(uv + float2(-h.x, h.y));
    c += sampleSource(uv + float2(h.x, h.y));
    c *= 0.125;
    if (g_push.first != 0) { // 문턱: 부드러운 무릎 (반지름 0.5), 밝기로 나눈 가중치가 반짝이는 점의 깜빡임을 줄인다
        PostConstants post = loadPost();
        float threshold = post.bloom.w;
        float l = luminance(c);
        float soft = clamp(l - threshold + 0.5, 0.0, 1.0);
        float contribution = max(soft * soft * 0.5, l - threshold) / max(l, 1e-4);
        c *= contribution / (1.0 + l * 0.05);
    }
    return float4(c, 1.0);
}

float4 PSBloomUp(PostVSOut input) : SV_Target
{
    float2 uv = input.position.xy * g_push.scale;
    float2 h = g_push.texel; // 작은 쪽 텍셀
    float3 c = 0.0;
    c += sampleSource(uv + float2(-h.x * 2.0, 0.0));
    c += sampleSource(uv + float2(h.x * 2.0, 0.0));
    c += sampleSource(uv + float2(0.0, -h.y * 2.0));
    c += sampleSource(uv + float2(0.0, h.y * 2.0));
    c += sampleSource(uv + float2(-h.x, -h.y)) * 2.0;
    c += sampleSource(uv + float2(h.x, -h.y)) * 2.0;
    c += sampleSource(uv + float2(-h.x, h.y)) * 2.0;
    c += sampleSource(uv + float2(h.x, h.y)) * 2.0;
    return float4(c / 12.0, 1.0); // 더하기 섞기: 이 단계의 내려온 값 위에 쌓인다
}

// ---------------------------------------------------------------- 톤 매핑 (AgX, Blender 4 기본과 같은 계열)
// Benjamin Wrensch의 다항식 근사 (MIT, iolite-engine.com/blog_posts/minimal_agx_implementation): 입력은 선형 Rec.709

float3 agxContrast(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 agx(float3 c) // 선형 → 표시 (감마가 들어간 sRGB에 가까운 값, 0..1)
{
    const float3x3 inset = float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                                    0.0784335999999992, 0.878468636469772, 0.0784336,
                                    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const float minEv = -12.47393, maxEv = 4.026069;
    c = mul(max(c, 0.0), inset);
    c = clamp(log2(max(c, 1e-10)), minEv, maxEv);
    c = (c - minEv) / (maxEv - minEv);
    return agxContrast(c);
}

float3 agxEotf(float3 c) // AgX 표시 값 → 선형 Rec.709 (밖으로 넓힌 원색을 되돌린다)
{
    const float3x3 outset = float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                                     -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                                     -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    c = mul(c, outset);
    return pow(max(c, 0.0), 2.2);
}

float3 toSrgb(float3 linearColor)
{
    linearColor = saturate(linearColor);
    return select(linearColor <= 0.0031308, linearColor * 12.92, 1.055 * pow(linearColor, 1.0 / 2.4) - 0.055);
}

// 32³ LUT를 1024×32 띠로 (가로 = 파랑 조각 × 32 + 빨강, 세로 = 초록). 파랑 두 조각을 섞는다
float3 applyLut(uint lut, float3 c)
{
    c = saturate(c);
    float slice = c.b * 31.0;
    float s0 = floor(slice);
    float s1 = min(s0 + 1.0, 31.0);
    float u = c.r * 31.0 + 0.5;
    float v = (c.g * 31.0 + 0.5) / 32.0;
    Texture2D t = g_textures[NonUniformResourceIndex(lut)];
    float3 a = t.SampleLevel(g_linearClamp, float2((s0 * 32.0 + u) / 1024.0, v), 0).rgb;
    float3 b = t.SampleLevel(g_linearClamp, float2((s1 * 32.0 + u) / 1024.0, v), 0).rgb;
    return lerp(a, b, slice - s0);
}

float hash12(float2 p) // 픽셀마다 고르게 흩어진 0..1 (Dave Hoskins hash without sine, MIT)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// FXAA (Timothy Lottes의 콘솔판 계열, 탭 9개): HDR 값은 밝기로 나눠(되돌릴 수 있는 Reinhard) 섞어 밝은 점이 번지지 않게 한다
float3 compress(float3 c) { return c / (1.0 + luminance(c)); }
float3 uncompress(float3 c) { return c / max(1.0 - luminance(c), 1e-4); }
float edgeLuma(float3 c) { return sqrt(luminance(compress(c))); }

float3 fxaa(float2 uv)
{
    float2 t = g_push.texel;
    float3 rgbM = sampleSource(uv);
    float lumaNW = edgeLuma(sampleSource(uv + float2(-t.x, -t.y)));
    float lumaNE = edgeLuma(sampleSource(uv + float2(t.x, -t.y)));
    float lumaSW = edgeLuma(sampleSource(uv + float2(-t.x, t.y)));
    float lumaSE = edgeLuma(sampleSource(uv + float2(t.x, t.y)));
    float lumaM = edgeLuma(rgbM);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125)) return rgbM; // 가장자리가 아니다

    float2 dir = float2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
    float reduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * 0.125), 1.0 / 128.0);
    float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpMin, -8.0, 8.0) * t;
    float3 a = 0.5 * (compress(sampleSource(uv + dir * (1.0 / 3.0 - 0.5))) + compress(sampleSource(uv + dir * (2.0 / 3.0 - 0.5))));
    float3 b = a * 0.5 + 0.25 * (compress(sampleSource(uv - dir * 0.5)) + compress(sampleSource(uv + dir * 0.5)));
    float lumaB = sqrt(luminance(b));
    return uncompress(lumaB < lumaMin || lumaB > lumaMax ? a : b);
}

float4 PSUber(PostVSOut input) : SV_Target
{
    PostConstants post = loadPost();
    uint flags = post.textures.z;
    float2 uv = input.position.xy * post.sceneSize.zw;
    float2 regionSize = post.region.zw - post.region.xy;
    float2 centered = (uv - post.region.xy) / max(regionSize, 1e-6) - 0.5; // 영역 가운데 0, 가장자리 ±0.5

    float3 color = (flags & kPostFxaa) ? fxaa(uv) : sampleSource(uv);
    if (post.effects.y > 0.0) { // 색수차: 빨강·파랑을 가장자리 쪽으로 벌린다 (가운데는 그대로)
        float2 offset = centered * dot(centered, centered) * post.effects.y * regionSize;
        color.r = sampleSource(uv + offset).r;
        color.b = sampleSource(uv - offset).b;
    }
    if (!(flags & kPostToneMap)) return float4(toSrgb(color), 1.0);

    if (post.textures.x != kNoTex) // 블룸 사슬 맨 위 (절반 해상도, 같은 uv)
        color += g_textures[NonUniformResourceIndex(post.textures.x)].SampleLevel(g_linearClamp, uv, 0).rgb * post.bloom.rgb * post.exposure.y;
    color *= post.exposure.x;

    float3 display = toSrgb(agxEotf(agx(color))); // 0..1 sRGB 값 (LUT·외부 보정 도구와 같은 공간)
    if (post.textures.y != kNoTex) display = applyLut(post.textures.y, display); // 프로젝트 LUT + 색맹 보정 (CPU가 합친 하나)
    if (post.exposure.z > 0.0) { // 비네트: 영역 가장자리를 어둡게 (부드러움 = 시작 반지름)
        float r = length(centered * float2(regionSize.x / max(regionSize.y, 1e-6), 1.0)) * 1.41421356;
        display *= 1.0 - post.exposure.z * smoothstep(post.exposure.w, 1.0 + post.exposure.w * 0.5, r);
    }
    float noise = hash12(input.position.xy + post.effects.z * 61.0) - 0.5;
    if (post.effects.x > 0.0) display += noise * post.effects.x * (1.0 - display) * 0.5; // 그레인: 어두운 곳에서 더 보인다
    if (flags & kPostDither) display += noise / 255.0;
    return float4(saturate(display), 1.0);
}
