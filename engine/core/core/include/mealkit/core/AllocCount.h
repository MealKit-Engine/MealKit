#pragma once

#include "mealkit/core/Types.h"

// Heap allocation counter (docs/ROADMAP.md optimization 9 "Frame allocation check"): dev game exes replace the global
// operator new with a counting one (AllocCount.cpp, MEALKIT_ALLOC_COUNT). Each thread counts its own allocations, so the
// main thread's render and UI work can be measured without job workers or the audio thread mixing in.
// The editor (mealkit.dll) and shipping builds don't count: counting() is false and threadCount() stays 0.
//   const u64 before = alloc::threadCount(); ... ; const u64 made = alloc::threadCount() - before;
namespace mealkit::alloc {

bool counting();
u64 threadCount(); // operator new calls on this thread so far

} // namespace mealkit::alloc
