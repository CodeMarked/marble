#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace marble::core::job {

using EntryPoint = void(*)(std::uintptr_t);

enum class Priority {
    Low,
    Normal,
    High,
    Critical
};

/// Counter shape aligned with §8.6.4; currently consumed inline.
class Counter {
public:
    Counter() noexcept = default;

    void add(unsigned int n = 1) noexcept {
        pending_.fetch_add(n, std::memory_order_relaxed);
    }

    void completeOne() noexcept {
        pending_.fetch_sub(1, std::memory_order_release);
    }

    [[nodiscard]] unsigned int pending() const noexcept {
        return pending_.load(std::memory_order_acquire);
    }

private:
    std::atomic<unsigned int> pending_{0};
};

struct Declaration {
    EntryPoint entryPoint{nullptr};
    std::uintptr_t param{0};
    Priority priority{Priority::Normal};
    Counter* counter{nullptr};
};

/// Baseline implementation: no worker threads; runs jobs on caller thread.
/// This preserves ADR-0011 while creating a stable API seam for future pools.
class InlineJobSystem {
public:
    void kickJob(Declaration const& decl) noexcept {
        if (decl.entryPoint == nullptr) {
            return;
        }
        if (decl.counter != nullptr) {
            decl.counter->add(1);
        }
        decl.entryPoint(decl.param);
        if (decl.counter != nullptr) {
            decl.counter->completeOne();
        }
    }

    void kickJobs(std::size_t count, Declaration const* decls) noexcept {
        if (decls == nullptr) {
            return;
        }
        for (std::size_t i = 0; i < count; ++i) {
            kickJob(decls[i]);
        }
    }

    void waitForCounter(Counter* counter) const noexcept {
        if (counter == nullptr) {
            return;
        }
        while (counter->pending() != 0u) {
            // Inline executor currently drains synchronously; this loop is a
            // forward-compatible wait seam for future worker-backed schedulers.
        }
    }

    void kickJobAndWait(Declaration const& decl) noexcept {
        kickJob(decl);
        waitForCounter(decl.counter);
    }

    void kickJobsAndWait(std::size_t count, Declaration const* decls, Counter* counter) noexcept {
        kickJobs(count, decls);
        waitForCounter(counter);
    }
};

/// Sequential scatter/gather helper with explicit batch decomposition.
/// A future worker scheduler can parallelize each batch without changing call sites.
inline void scatterGather(std::size_t totalCount,
                          std::size_t batchSize,
                          std::function<void(std::size_t start, std::size_t count)> const& batchFn) {
    if (batchSize == 0 || !batchFn) {
        return;
    }
    std::size_t begin = 0;
    while (begin < totalCount) {
        const std::size_t count = ((begin + batchSize) <= totalCount) ? batchSize : (totalCount - begin);
        batchFn(begin, count);
        begin += count;
    }
}

} // namespace marble::core::job
