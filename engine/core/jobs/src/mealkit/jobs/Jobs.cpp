#include "mealkit/jobs/Jobs.h"

#include "mealkit/jobs/JoltAllocator.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>

#include <algorithm>
#include <mutex>
#include <thread>

namespace mealkit::jobs {
namespace {

std::mutex g_lock; // guards pool creation/destruction (acquire/release may come from a simulation thread)
u32 g_users = 0;
JPH::JobSystemThreadPool* g_pool = nullptr;

// Physics (one barrier per world step, several worlds with shadow/rewind) + render and loading jobs
constexpr u32 kMaxJobs = 4096;
constexpr u32 kMaxBarriers = 64;

} // namespace

void acquire() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_users++ > 0) return;
    installJoltAllocator(); // the pool allocates through Jolt's hooks, shared with physics (JoltAllocator.h)
    const u32 cores = std::max(1u, std::thread::hardware_concurrency());
    const u32 workers = std::clamp(cores - 1, 1u, 6u); // leave one for the main thread
    g_pool = new JPH::JobSystemThreadPool(kMaxJobs, kMaxBarriers, static_cast<int>(workers));
}

void release() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_users == 0 || --g_users > 0) return;
    delete g_pool;
    g_pool = nullptr;
}

u32 workerCount() { return g_pool ? static_cast<u32>(g_pool->GetMaxConcurrency() - 1) : 0; }

void* joltJobSystem() { return g_pool; }

void parallelFor(u32 count, u32 minChunk, const std::function<void(u32, u32)>& fn) {
    if (count == 0) return;
    minChunk = std::max(minChunk, 1u);
    const u32 lanes = workerCount() + 1;
    const u32 chunks = std::min(lanes, (count + minChunk - 1) / minChunk);
    if (chunks <= 1) {
        fn(0, count);
        return;
    }
    const u32 per = (count + chunks - 1) / chunks;
    JPH::JobSystem::Barrier* barrier = g_pool->CreateBarrier();
    // The calling thread takes the first chunk; the rest go to workers
    for (u32 begin = per; begin < count; begin += per) {
        const u32 end = std::min(begin + per, count);
        JPH::JobHandle job = g_pool->CreateJob("parallelFor", JPH::Color::sGreen, [&fn, begin, end] { fn(begin, end); });
        barrier->AddJob(job);
    }
    fn(0, std::min(per, count));
    g_pool->WaitForJobs(barrier);
    g_pool->DestroyBarrier(barrier);
}

struct Task::Impl {
    JPH::JobHandle handle; // empty when the work ran inline
};

Task::Task(Task&&) noexcept = default;
Task& Task::operator=(Task&& other) noexcept {
    if (this != &other) {
        wait();
        m = std::move(other.m);
    }
    return *this;
}
Task::~Task() { wait(); }

bool Task::isReady() const { return !m || !m->handle.IsValid() || m->handle.IsDone(); }

void Task::wait() {
    if (!m) return;
    if (!isReady()) {
        JPH::JobSystem::Barrier* barrier = g_pool->CreateBarrier();
        barrier->AddJob(m->handle);
        g_pool->WaitForJobs(barrier);
        g_pool->DestroyBarrier(barrier);
    }
    m->handle = JPH::JobHandle(); // give the job back to the pool now
}

Task run(std::function<void()> fn) {
    Task task;
    task.m = std::make_unique<Task::Impl>();
    if (!g_pool) {
        fn();
        return task;
    }
    task.m->handle = g_pool->CreateJob("task", JPH::Color::sBlue, std::move(fn));
    return task;
}

} // namespace mealkit::jobs
