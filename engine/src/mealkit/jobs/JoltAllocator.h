#pragma once

// Jolt's allocation hooks (JPH::Allocate, Free, ...) set once for the whole process, counted under the memory tag "jolt"
// (core/Memory.h). Both users of Jolt call it before creating any Jolt object: the job pool (jobs/Jobs.cpp) and physics
// (physics/PhysicsWorld.cpp). Installing them in one place matters: memory one set of hooks allocated must never reach
// another set's free. Thread safe, and calls after the first do nothing. Internal: game code doesn't include it.
namespace mealkit::jobs {

void installJoltAllocator();

} // namespace mealkit::jobs
