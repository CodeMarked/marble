#include "input/Hid.hpp"

#include <array>

namespace {

constexpr marble::input::ButtonBits kA = 0x0001u;
constexpr marble::input::ButtonBits kB = 0x0002u;

} // namespace

int main() {
    marble::input::ButtonStateTracker buttons;

    // Chord detector with grace: A then B one frame later should trigger once.
    marble::input::ChordDetector chord(kA | kB, 2);
    buttons.update(0u);                 // frame 0
    if (chord.update(buttons, 0)) return 1;
    buttons.update(kA);                 // frame 1
    if (chord.update(buttons, 1)) return 2;
    buttons.update(kA | kB);            // frame 2
    if (!chord.update(buttons, 2)) return 3;
    if (chord.update(buttons, 3)) return 4; // held chord should not re-emit
    buttons.update(0u);                 // release
    (void)chord.update(buttons, 4);

    // Rapid tap: first down initializes, second fast down becomes valid.
    marble::input::ButtonTapDetector tap(kA, 0.25f);
    buttons.update(kA);
    if (tap.update(buttons, 0.00f)) return 5;
    buttons.update(0u);
    (void)tap.update(buttons, 0.05f);
    buttons.update(kA);
    if (!tap.update(buttons, 0.15f)) return 6;
    buttons.update(0u);
    (void)tap.update(buttons, 0.50f);
    if (tap.isValid()) return 7;

    // Sequence detector: A-B-A within dtMax should complete once.
    std::array<marble::input::ButtonBits, 3> seq{kA, kB, kA};
    marble::input::ButtonSequenceDetector<3> seqDet(seq, 3, 1.0f);
    buttons.update(kA);
    if (seqDet.update(buttons, 0.00f)) return 8;
    buttons.update(0u);
    (void)seqDet.update(buttons, 0.05f);
    buttons.update(kB);
    if (seqDet.update(buttons, 0.30f)) return 9;
    buttons.update(0u);
    (void)seqDet.update(buttons, 0.35f);
    buttons.update(kA);
    if (!seqDet.update(buttons, 0.60f)) return 10;

    // Wrong button should reset sequence.
    buttons.update(kA);
    (void)seqDet.update(buttons, 1.00f);
    buttons.update(0u);
    (void)seqDet.update(buttons, 1.05f);
    buttons.update(kA); // expected B, this should reset
    if (seqDet.update(buttons, 1.20f)) return 11;

    return 0;
}
