#include "core/Time.hpp"

#include <cstdlib>

int main() {
    // HiRes clock sanity.
    const std::uint64_t f = marble::core::HiResClock::frequency();
    static_assert(marble::core::HiResClock::frequency() > 0, "HiResClock frequency must be positive");
    if (marble::core::HiResClock::ticksToSeconds(f) < 0.999 || marble::core::HiResClock::ticksToSeconds(f) > 1.001) {
        return 2;
    }

    marble::core::FrameDeltaEstimator<4> est(1.0 / 60.0, 1.0);

    // Normal frame deltas should pass through averaging with bounded values.
    const double d0 = est.next(1.0 / 60.0);
    const double d1 = est.next(1.0 / 30.0);
    if (d0 <= 0.0 || d1 <= 0.0) {
        return 3;
    }

    // Breakpoint-like spike should be clamped to target before averaging.
    const double d2 = est.next(5.0);
    if (d2 > 0.2) {
        return 4;
    }

    // Negative measurements should be sanitized to zero and still stay non-negative.
    const double d3 = est.next(-1.0);
    if (d3 < 0.0) {
        return 5;
    }

    return 0;
}
