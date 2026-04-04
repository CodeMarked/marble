#pragma once

namespace marble::game_shared {

/// Rising edge while `held` is true (Esc / BackSelect aggregate).
[[nodiscard]] inline bool pauseMenuToggleEdge(bool held, bool& wasHeld) noexcept
{
    bool const edge = held && !wasHeld;
    wasHeld = held;
    return edge;
}

/// Rising edge on Q (return to app launcher). Use only while the pause overlay is active.
[[nodiscard]] inline bool pauseMenuReturnToMainMenuQEdge(bool qDown, bool& qWasDown) noexcept
{
    bool const edge = qDown && !qWasDown;
    qWasDown = qDown;
    return edge;
}

/// Rising edge on gamepad B (return to app launcher). Use only while the pause overlay is active.
[[nodiscard]] inline bool pauseMenuReturnToMainMenuPadBEdge(bool bDown, bool& bWasDown) noexcept
{
    bool const edge = bDown && !bWasDown;
    bWasDown = bDown;
    return edge;
}

/// Q or pad B rising edge → leave gameplay for the app main menu. Call only when paused.
[[nodiscard]] inline bool pauseMenuWantsReturnToLauncher(
    bool qDown,
    bool padBDown,
    bool& qWasDown,
    bool& padBWasDown) noexcept
{
    bool const qEdge = pauseMenuReturnToMainMenuQEdge(qDown, qWasDown);
    bool const bEdge = pauseMenuReturnToMainMenuPadBEdge(padBDown, padBWasDown);
    return qEdge || bEdge;
}

/// Call when not paused so a held Q/B does not fire an edge the instant pause opens.
inline void syncPauseMenuReturnEdgeState(
    bool qDown,
    bool padBDown,
    bool& qWasDown,
    bool& padBWasDown) noexcept
{
    qWasDown = qDown;
    padBWasDown = padBDown;
}

} // namespace marble::game_shared
