#pragma once

#include "core/StringId.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

/// Integer argument passed across the native/script boundary (§16.9 seam).
using ScriptArg = std::int32_t;
using ScriptProc = void (*)(void* userData, ScriptArg arg);

struct ScriptBinding {
    core::StringId id{};
    ScriptProc proc{};
    void* userData{};
};

/// Fixed-capacity table of named native callables; no VM or bytecode (bindings seam only).
template <std::size_t MaxBindings>
class ScriptHost {
public:
    static_assert(MaxBindings > 0, "ScriptHost requires positive capacity");

    [[nodiscard]] bool registerBinding(core::StringId id, ScriptProc proc, void* userData) noexcept {
        if (id.value == 0u || proc == nullptr) {
            return false;
        }
        if (findIndex(id) < count_) {
            return false;
        }
        if (count_ >= MaxBindings) {
            return false;
        }
        bindings_[count_++] = ScriptBinding{id, proc, userData};
        return true;
    }

    [[nodiscard]] bool invoke(core::StringId id, ScriptArg arg) noexcept {
        const std::size_t i = findIndex(id);
        if (i >= count_) {
            return false;
        }
        ScriptBinding const& b = bindings_[i];
        b.proc(b.userData, arg);
        return true;
    }

    [[nodiscard]] std::size_t bindingCount() const noexcept { return count_; }

    void clear() noexcept { count_ = 0; }

private:
    [[nodiscard]] std::size_t findIndex(core::StringId id) const noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (bindings_[i].id == id) {
                return i;
            }
        }
        return MaxBindings;
    }

    std::array<ScriptBinding, MaxBindings> bindings_{};
    std::size_t count_{};
};

/// Top-level session phases for high-level flow (§16.10).
enum class GameFlowPhase : std::uint8_t {
    Boot,
    Loading,
    Playing,
    Paused,
    Exiting
};

[[nodiscard]] constexpr bool gameFlowTransitionAllowed(GameFlowPhase from, GameFlowPhase to) noexcept {
    if (from == to) {
        return false;
    }
    switch (from) {
    case GameFlowPhase::Boot:
        return to == GameFlowPhase::Loading;
    case GameFlowPhase::Loading:
        return to == GameFlowPhase::Playing;
    case GameFlowPhase::Playing:
        return to == GameFlowPhase::Paused || to == GameFlowPhase::Exiting;
    case GameFlowPhase::Paused:
        return to == GameFlowPhase::Playing || to == GameFlowPhase::Exiting;
    case GameFlowPhase::Exiting:
        return false;
    }
    return false;
}

class GameFlowController {
public:
    [[nodiscard]] GameFlowPhase phase() const noexcept { return phase_; }

    [[nodiscard]] bool tryTransition(GameFlowPhase to) noexcept {
        if (!gameFlowTransitionAllowed(phase_, to)) {
            return false;
        }
        phase_ = to;
        return true;
    }

    void reset() noexcept { phase_ = GameFlowPhase::Boot; }

private:
    GameFlowPhase phase_{GameFlowPhase::Boot};
};

} // namespace marble::gameplay
