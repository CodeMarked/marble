#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace marble::core {

/// Thin wrapper over a monotonic high-resolution clock (book §8.5.3).
class HiResClock {
public:
    using clock = std::chrono::steady_clock;

    [[nodiscard]] static std::uint64_t readTicks() noexcept {
        const auto now = clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    [[nodiscard]] static constexpr std::uint64_t frequency() noexcept {
        return 1'000'000'000ull; // nanoseconds per second
    }

    [[nodiscard]] static constexpr double ticksToSeconds(std::uint64_t ticks) noexcept {
        return static_cast<double>(ticks) / static_cast<double>(frequency());
    }
};

/// Delta-time estimator policy (book §8.5.2.3 and §8.5.5).
/// - Uses a short running average to soften transient spikes.
/// - Clamps suspiciously large frame deltas (e.g., breakpoints) to target delta.
template <std::size_t Window = 4>
class FrameDeltaEstimator {
public:
    static_assert(Window > 0, "Window must be > 0");

    explicit FrameDeltaEstimator(double targetDeltaSeconds = 1.0 / 60.0,
                                 double breakpointSpikeSeconds = 1.0) noexcept
        : targetDeltaSeconds_(targetDeltaSeconds > 0.0 ? targetDeltaSeconds : 1.0 / 60.0),
          breakpointSpikeSeconds_(breakpointSpikeSeconds > 0.0 ? breakpointSpikeSeconds : 1.0) {
        samples_.fill(targetDeltaSeconds_);
    }

    [[nodiscard]] double next(double measuredDeltaSeconds) noexcept {
        double dt = measuredDeltaSeconds;
        if (dt < 0.0) {
            dt = 0.0;
        }
        if (dt > breakpointSpikeSeconds_) {
            dt = targetDeltaSeconds_;
        }

        samples_[cursor_] = dt;
        cursor_ = (cursor_ + 1) % Window;
        if (count_ < Window) {
            ++count_;
        }

        double sum = 0.0;
        for (std::size_t i = 0; i < count_; ++i) {
            sum += samples_[i];
        }
        const double avg = sum / static_cast<double>(count_);
        return std::max(0.0, avg);
    }

private:
    double targetDeltaSeconds_{1.0 / 60.0};
    double breakpointSpikeSeconds_{1.0};
    std::array<double, Window> samples_{};
    std::size_t cursor_{0};
    std::size_t count_{0};
};

} // namespace marble::core
