#pragma once

namespace marble::game_shared {

/// Rising edge while `held` is true (Esc / BackSelect aggregate).
[[nodiscard]] inline bool pauseMenuToggleEdge(bool held, bool& wasHeld) noexcept
{
    bool const edge = held && !wasHeld;
    wasHeld = held;
    return edge;
}

/// Rising edge on Q (main menu from pause).
[[nodiscard]] inline bool pauseMenuReturnToMainMenuQEdge(bool qDown, bool& qWasDown) noexcept
{
    bool const edge = qDown && !qWasDown;
    qWasDown = qDown;
    return edge;
}

/// Rising edge on gamepad B (main menu from pause).
[[nodiscard]] inline bool pauseMenuReturnToMainMenuPadBEdge(bool bDown, bool& bWasDown) noexcept
{
    bool const edge = bDown && !bWasDown;
    bWasDown = bDown;
    return edge;
}

} // namespace marble::game_shared
