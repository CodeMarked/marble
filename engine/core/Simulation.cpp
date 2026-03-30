#include "core/Simulation.hpp"

#include <utility>

namespace marble::core {

FixedStepSimulationPhase::FixedStepSimulationPhase(double fixedDeltaSeconds, unsigned maxSubsteps)
    : fixedDeltaSeconds_(fixedDeltaSeconds > 0.0 ? fixedDeltaSeconds : 1.0 / 60.0),
      maxSubsteps_(maxSubsteps > 0 ? maxSubsteps : 1) {}

void FixedStepSimulationPhase::setStepCallback(StepCallback callback) {
    callback_ = std::move(callback);
}

void FixedStepSimulationPhase::tick(const Engine::FrameContext& ctx) {
    constexpr double kMaxAccumulatedDeltaSeconds = 0.25;
    const double clampedDelta = ctx.deltaSeconds > kMaxAccumulatedDeltaSeconds ? kMaxAccumulatedDeltaSeconds : ctx.deltaSeconds;
    accumulatorSeconds_ += clampedDelta;

    unsigned executedSubsteps = 0;
    while (accumulatorSeconds_ >= fixedDeltaSeconds_ && executedSubsteps < maxSubsteps_) {
        Engine::FrameContext stepCtx = ctx;
        stepCtx.deltaSeconds = fixedDeltaSeconds_;
        if (callback_) {
            callback_(stepCtx);
        }
        accumulatorSeconds_ -= fixedDeltaSeconds_;
        ++executedSubsteps;
        ++totalSimulationSteps_;
    }

    if (accumulatorSeconds_ >= fixedDeltaSeconds_) {
        // Drop excess accumulated time to avoid runaway catch-up loops.
        accumulatorSeconds_ = 0.0;
        ++droppedStepBatches_;
    }
}

unsigned long long FixedStepSimulationPhase::totalSimulationSteps() const {
    return totalSimulationSteps_;
}

unsigned long long FixedStepSimulationPhase::droppedStepBatches() const {
    return droppedStepBatches_;
}

} // namespace marble::core
