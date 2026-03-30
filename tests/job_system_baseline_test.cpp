#include "core/JobSystem.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

struct SumState {
    int total{0};
};

void addValue(std::uintptr_t param) {
    auto* state = reinterpret_cast<SumState*>(param);
    state->total += 3;
}

} // namespace

int main() {
    marble::core::job::InlineJobSystem js;
    marble::core::job::Counter counter;

    // Single job.
    SumState one{};
    marble::core::job::Declaration d1{};
    d1.entryPoint = &addValue;
    d1.param = reinterpret_cast<std::uintptr_t>(&one);
    d1.counter = &counter;
    js.kickJobAndWait(d1);
    if (one.total != 3 || counter.pending() != 0u) {
        return 1;
    }

    // Batch jobs.
    SumState two{};
    std::array<marble::core::job::Declaration, 4> batch{};
    for (auto& d : batch) {
        d.entryPoint = &addValue;
        d.param = reinterpret_cast<std::uintptr_t>(&two);
        d.counter = &counter;
    }
    js.kickJobsAndWait(batch.size(), batch.data(), &counter);
    if (two.total != 12 || counter.pending() != 0u) {
        return 2;
    }

    // Scatter/gather partitioning should cover every item exactly once.
    std::vector<int> marks(10, 0);
    marble::core::job::scatterGather(
        marks.size(),
        3,
        [&marks](std::size_t start, std::size_t count) {
            for (std::size_t i = start; i < start + count; ++i) {
                marks[i] += 1;
            }
        });
    for (int v : marks) {
        if (v != 1) {
            return 3;
        }
    }

    return 0;
}
