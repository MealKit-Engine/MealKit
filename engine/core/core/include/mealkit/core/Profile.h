#pragma once

// Profiling markers (Tracy). Active only in MEALKIT_PROFILE=ON builds (build.bat Profile);
// normal builds generate no code.
//   MEALKIT_ZONE("name");   // times the current scope
//   MEALKIT_FRAME_MARK();    // end of a frame
#include <tracy/Tracy.hpp>

#define MEALKIT_ZONE(name) ZoneScopedN(name)
#define MEALKIT_FRAME_MARK() FrameMark
