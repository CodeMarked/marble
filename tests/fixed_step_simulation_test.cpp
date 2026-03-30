#include "core/Engine.hpp"
#include "core/Simulation.hpp"

int main() {
    marble::core::FixedStepSimulationPhase phase(1.0 / 60.0, 8);
    unsigned long long callbackCount = 0;
    phase.setStepCallback([&callbackCount](const marble::core::Engine::FrameContext&) {
        ++callbackCount;
    });

    marble::core::Engine::FrameContext frame {};
    frame.frameIndex = 0;

    // 120 ms at 60 Hz should produce 7 fixed steps.
    frame.deltaSeconds = 0.12;
    phase.tick(frame);
    if (callbackCount != 7) {
        return 1;
    }

    // Another 50 ms should produce 3 more steps.
    frame.frameIndex = 1;
    frame.deltaSeconds = 0.05;
    phase.tick(frame);
    if (callbackCount != 10) {
        return 2;
    }

    // Oversized delta with maxSubsteps=8 should cap and mark a dropped batch.
    frame.frameIndex = 2;
    frame.deltaSeconds = 1.0;
    phase.tick(frame);
    if (phase.droppedStepBatches() == 0) {
        return 3;
    }

    return 0;
}
