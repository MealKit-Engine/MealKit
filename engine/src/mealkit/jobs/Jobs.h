#pragma once

#include "mealkit/core/Types.h"

#include <functional>
#include <memory>

// The engine's one Job System (docs/CODING_MODEL.md "Things that take time"): short parallel computation on one shared worker pool.
// The pool is Jolt's JobSystemThreadPool, so physics steps and render preparation use the same workers (no oversubscription).
// Not for sim/ code. Results of a parallelFor must not depend on how the range is split (each index writes only its own output).
namespace mealkit::jobs {

// Reference-counted pool lifetime. The first acquire starts the workers (core count - 1, at most 6), the last release joins them.
// Called by subsystems that use jobs (PhysicsWorld::init, Renderer::init); explicit so threads never end inside DLL unload.
void acquire();
void release();

// Worker threads (0 when no pool is acquired). Work also runs on the calling thread, so concurrency is workerCount() + 1.
u32 workerCount();

// Runs fn(begin, end) over [0, count) in chunks of at least minChunk on the workers and the calling thread, and returns when all are done.
// Runs inline when there is no pool or the range is small. Don't call it from inside a job.
void parallelFor(u32 count, u32 minChunk, const std::function<void(u32 begin, u32 end)>& fn);

// One background job, polled with isReady() (no callbacks). Destroying an unfinished Task waits for it.
// A Task must be waited for or destroyed before the release() that ends the pool.
class Task {
public:
    Task() = default;
    Task(Task&&) noexcept;
    Task& operator=(Task&&) noexcept;
    ~Task();

    bool valid() const { return m != nullptr; }
    bool isReady() const;
    void wait();

    struct Impl;

private:
    friend Task run(std::function<void()> fn);
    std::unique_ptr<Impl> m;
};

// Starts fn on a worker (inline when there is no pool, so the returned Task is ready at once)
Task run(std::function<void()> fn);

// For physics::PhysicsWorld: the pool as JPH::JobSystem* (nullptr without a pool). Jolt types stay out of this header.
void* joltJobSystem();

} // namespace mealkit::jobs
