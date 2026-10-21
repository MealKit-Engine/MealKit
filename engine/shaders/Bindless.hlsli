// RHI 바인딩 모양 (mealkit/rhi/Rhi.h, docs/DECISIONS.md 2026-10-19 "RHI 바인딩 모양"). 모든 셰이더가 같이 쓴다.
// DX12: 루트 상수 b0 + 서술자 표 두 개(space1 버퍼, space2 텍스처) + 고정 샘플러 s0
// Vulkan: 푸시 상수 + 세트 0의 바인딩 0(버퍼 배열), 1(텍스처 배열), 2·3(고정 샘플러)
// 구조체는 Load로 필드마다 읽는다 (두 API의 구조체 배치 규칙 차이를 피한다).

[[vk::binding(0, 0)]] ByteAddressBuffer g_buffers[] : register(t0, space1);
[[vk::binding(1, 0)]] Texture2D g_textures[] : register(t0, space2);
[[vk::binding(2, 0)]] SamplerState g_linearClamp : register(s0);
[[vk::binding(3, 0)]] SamplerState g_linearWrap : register(s1); // 모델 텍스처 (반복, 밉, 이방성 8)

float3 loadFloat3(ByteAddressBuffer b, uint offset) { return asfloat(b.Load3(offset)); }
float4 loadFloat4(ByteAddressBuffer b, uint offset) { return asfloat(b.Load4(offset)); }
