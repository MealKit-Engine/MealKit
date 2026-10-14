// Aster forward shading.
// 인스턴스 데이터(위치/회전/스케일/색)를 StructuredBuffer로 받아 정점 셰이더에서 변환한다.
// CPU는 행렬을 만들지 않고 48바이트 인스턴스만 쓴다. 조명은 하프 램버트 태양광 + 반구 환경광 + 지수 안개.

struct Instance
{
    float3 position;
    uint color;     // RGBA8 (sRGB), A는 사용 안 함
    float4 rotation; // 쿼터니언
    float3 scale;
    uint flags;     // bit0: 체커 패턴, bit1: 선택 강조
};

cbuffer FrameConstants : register(b0)
{
    row_major float4x4 viewProj;
    float4 cameraPos;
    float4 sunDir;      // 태양을 향하는 방향
    float4 sunColor;
    float4 skyColor;
    float4 groundColor;
    float4 fogColor;    // a = 안개 밀도
};

StructuredBuffer<Instance> instances : register(t0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR; // 정점 색 (sRGB, glTF 재질 색)
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float3 albedo : COLOR;
    nointerpolation uint flags : FLAGS;
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
    Instance inst = instances[input.instanceId];
    float3 world = rotateByQuat(inst.rotation, input.position * inst.scale) + inst.position;

    VSOutput o;
    o.position = mul(float4(world, 1.0), viewProj);
    o.worldPos = world;
    o.normal = rotateByQuat(inst.rotation, input.normal / inst.scale);
    o.albedo = unpackAlbedo(inst.color) * (input.color.rgb * input.color.rgb);
    o.flags = inst.flags;
    return o;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 n = normalize(input.normal);
    float3 albedo = input.albedo;

    if (input.flags & 1u)
    {
        float2 cell = floor(input.worldPos.xz * 0.25);
        float checker = fmod(abs(cell.x + cell.y), 2.0);
        albedo *= lerp(1.0, 0.85, checker);
    }

    float halfLambert = dot(n, sunDir.xyz) * 0.5 + 0.5;
    float3 ambient = lerp(groundColor.rgb, skyColor.rgb, n.y * 0.5 + 0.5);
    float3 color = albedo * (sunColor.rgb * (halfLambert * halfLambert) + ambient);

    if (input.flags & 2u) // 에디터 선택 강조
        color = lerp(color, float3(1.0, 0.45, 0.05), 0.45) + float3(0.08, 0.03, 0.0);

    float dist = distance(input.worldPos, cameraPos.xyz);
    float fog = exp(-dist * fogColor.a);
    return float4(lerp(fogColor.rgb, color, fog), 1.0);
}
