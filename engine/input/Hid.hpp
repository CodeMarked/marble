#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>

namespace marble::input {

using ButtonBits = std::uint32_t;

struct ButtonEvents {
    ButtonBits downs{0};
    ButtonBits ups{0};
};

/// Tracks packed button bits and emits per-frame up/down events.
class ButtonStateTracker {
public:
    void update(ButtonBits currentBits) noexcept {
        prev_ = current_;
        current_ = currentBits;
        const ButtonBits changed = current_ ^ prev_;
        downs_ = changed & current_;
        ups_ = changed & (~current_);
    }

    [[nodiscard]] ButtonBits current() const noexcept { return current_; }
    [[nodiscard]] ButtonBits previous() const noexcept { return prev_; }
    [[nodiscard]] ButtonEvents events() const noexcept { return ButtonEvents{downs_, ups_}; }

    [[nodiscard]] bool isDown(ButtonBits mask) const noexcept { return (current_ & mask) == mask; }
    [[nodiscard]] bool wentDown(ButtonBits mask) const noexcept { return (downs_ & mask) != 0; }
    [[nodiscard]] bool wentUp(ButtonBits mask) const noexcept { return (ups_ & mask) != 0; }

    /// True when every button in the mask is currently held.
    [[nodiscard]] bool chordDown(ButtonBits chordMask) const noexcept {
        return chordMask != 0 && (current_ & chordMask) == chordMask;
    }

private:
    ButtonBits current_{0};
    ButtonBits prev_{0};
    ButtonBits downs_{0};
    ButtonBits ups_{0};
};

/// Centered dead-zone for two-way controls (e.g., stick axis).
[[nodiscard]] inline float applyCenteredDeadZone(float value, float deadZone) noexcept {
    if (deadZone <= 0.0f) {
        return value;
    }
    return (std::fabs(value) <= deadZone) ? 0.0f : value;
}

/// One-sided dead-zone for positive-range controls (e.g., trigger).
[[nodiscard]] inline float applyPositiveDeadZone(float value, float deadZone) noexcept {
    if (deadZone <= 0.0f) {
        return value;
    }
    if (value < 0.0f) {
        return 0.0f;
    }
    return (value <= deadZone) ? 0.0f : value;
}

/// First-order low-pass filter from Chapter 9 equation 9.1 / 9.2.
[[nodiscard]] inline float lowPassFilter(float unfilteredInput,
                                         float lastFilteredInput,
                                         float rc,
                                         float dt) noexcept {
    if (dt <= 0.0f) {
        return lastFilteredInput;
    }
    if (rc <= 0.0f) {
        return unfilteredInput;
    }
    const float a = dt / (rc + dt);
    return (1.0f - a) * lastFilteredInput + a * unfilteredInput;
}

template <typename T, std::size_t Size>
class MovingAverage {
public:
    static_assert(Size > 0, "Size must be > 0");

    void addSample(T value) noexcept {
        if (sampleCount_ == Size) {
            sum_ -= samples_[cursor_];
        } else {
            ++sampleCount_;
        }
        samples_[cursor_] = value;
        sum_ += value;
        cursor_ = (cursor_ + 1) % Size;
    }

    [[nodiscard]] float currentAverage() const noexcept {
        if (sampleCount_ == 0) {
            return 0.0f;
        }
        return static_cast<float>(sum_) / static_cast<float>(sampleCount_);
    }

private:
    std::array<T, Size> samples_{};
    T sum_{static_cast<T>(0)};
    std::size_t cursor_{0};
    std::size_t sampleCount_{0};
};

/// Chord detector with a small frame-based grace window between component presses.
/// This supports "almost simultaneous" human input for multi-button chords.
class ChordDetector {
public:
    explicit ChordDetector(ButtonBits chordMask, std::uint64_t graceFrames = 2) noexcept
        : chordMask_(chordMask), graceFrames_(graceFrames) {
        lastDownFrame_.fill(kInvalidFrame);
    }

    [[nodiscard]] bool update(ButtonStateTracker const& tracker, std::uint64_t frameIndex) noexcept {
        const ButtonBits downs = tracker.events().downs;
        for (std::size_t bit = 0; bit < 32; ++bit) {
            const ButtonBits mask = (ButtonBits{1u} << bit);
            if ((downs & mask) != 0u) {
                lastDownFrame_[bit] = frameIndex;
            }
        }

        if (!tracker.chordDown(chordMask_)) {
            emittedWhileHeld_ = false;
            return false;
        }

        std::uint64_t minFrame = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t maxFrame = 0;
        for (std::size_t bit = 0; bit < 32; ++bit) {
            const ButtonBits mask = (ButtonBits{1u} << bit);
            if ((chordMask_ & mask) == 0u) {
                continue;
            }
            const std::uint64_t t = lastDownFrame_[bit];
            if (t == kInvalidFrame) {
                return false;
            }
            if (t < minFrame) minFrame = t;
            if (t > maxFrame) maxFrame = t;
        }

        if ((maxFrame - minFrame) > graceFrames_) {
            return false;
        }

        if (emittedWhileHeld_) {
            return false;
        }
        emittedWhileHeld_ = true;
        return true;
    }

private:
    static constexpr std::uint64_t kInvalidFrame = std::numeric_limits<std::uint64_t>::max();
    ButtonBits chordMask_{0};
    std::uint64_t graceFrames_{2};
    std::array<std::uint64_t, 32> lastDownFrame_{};
    bool emittedWhileHeld_{false};
};

/// Detects rapid tapping of one button.
class ButtonTapDetector {
public:
    ButtonTapDetector(ButtonBits buttonMask, float dtMaxSeconds) noexcept
        : buttonMask_(buttonMask), dtMaxSeconds_(dtMaxSeconds > 0.0f ? dtMaxSeconds : 0.0f) {}

    [[nodiscard]] bool update(ButtonStateTracker const& tracker, float nowSeconds) noexcept {
        if (tracker.wentDown(buttonMask_)) {
            const float dt = nowSeconds - lastDownSeconds_;
            const bool valid = initialized_ && dt > 0.0f && dt <= dtMaxSeconds_;
            lastDownSeconds_ = nowSeconds;
            initialized_ = true;
            isValid_ = valid;
        } else if (initialized_ && (nowSeconds - lastDownSeconds_) > dtMaxSeconds_) {
            isValid_ = false;
        }
        return isValid_;
    }

    [[nodiscard]] bool isValid() const noexcept { return isValid_; }

private:
    ButtonBits buttonMask_{0};
    float dtMaxSeconds_{0.0f};
    float lastDownSeconds_{0.0f};
    bool initialized_{false};
    bool isValid_{false};
};

/// Ordered button-sequence detector (e.g., A-B-A) with a max total duration.
template <std::size_t MaxButtons = 8>
class ButtonSequenceDetector {
public:
    ButtonSequenceDetector(std::array<ButtonBits, MaxButtons> sequenceMasks,
                           std::size_t count,
                           float dtMaxSeconds) noexcept
        : sequenceMasks_(sequenceMasks),
          count_(count <= MaxButtons ? count : MaxButtons),
          dtMaxSeconds_(dtMaxSeconds > 0.0f ? dtMaxSeconds : 0.0f) {}

    [[nodiscard]] bool update(ButtonStateTracker const& tracker, float nowSeconds) noexcept {
        const ButtonBits downs = tracker.events().downs;
        if (count_ == 0 || downs == 0u) {
            return false;
        }

        const ButtonBits expected = sequenceMasks_[index_];
        const ButtonBits otherDowns = downs & (~expected);
        if (otherDowns != 0u) {
            reset();
            return false;
        }
        if ((downs & expected) == 0u) {
            return false;
        }

        if (index_ == 0) {
            startSeconds_ = nowSeconds;
            ++index_;
            return false;
        }

        if ((nowSeconds - startSeconds_) > dtMaxSeconds_) {
            reset();
            return false;
        }

        ++index_;
        if (index_ == count_) {
            reset();
            return true;
        }
        return false;
    }

    void reset() noexcept {
        index_ = 0;
        startSeconds_ = 0.0f;
    }

private:
    std::array<ButtonBits, MaxButtons> sequenceMasks_{};
    std::size_t count_{0};
    float dtMaxSeconds_{0.0f};
    std::size_t index_{0};
    float startSeconds_{0.0f};
};

} // namespace marble::input
