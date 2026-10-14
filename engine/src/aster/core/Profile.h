#pragma once

// 프로파일링 마커 (Tracy). ASTER_PROFILE=ON 빌드(build.bat Profile)에서만 동작하고,
// 일반 빌드에서는 아무 코드도 만들지 않는다.
//   ASTER_ZONE("이름");   // 현재 스코프의 시간을 잰다
//   ASTER_FRAME_MARK();    // 한 프레임의 끝
#include <tracy/Tracy.hpp>

#define ASTER_ZONE(name) ZoneScopedN(name)
#define ASTER_FRAME_MARK() FrameMark
