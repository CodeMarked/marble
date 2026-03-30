#pragma once

#include "core/Engine.hpp"

#include <functional>

namespace marble::core {

/// Fixed-step simulation phase suitable for deterministic baseline updates.
class FixedStepSimulationPhase final : public Engine::ISimulationPhase {
public:
    using StepCallback = std::function<void(const Engine::FrameContext&)>;

    explicit FixedStepSimulationPhase(double fixedDeltaSeconds = 1.0 / 60.0, unsigned maxSubsteps = 8);

    void setStepCallback(StepCallback callback);
    void tick(const Engine::FrameContext& ctx) override;

    unsigned long long totalSimulationSteps() const;
    unsigned long long droppedStepBatches() const;

private:
    double fixedDeltaSeconds_ = 1.0 / 60.0;
    unsigned maxSubsteps_ = 8;
    double accumulatorSeconds_ = 0.0;
    unsigned long long totalSimulationSteps_ = 0;
    unsigned long long droppedStepBatches_ = 0;
    StepCallback callback_;
};

} // namespace marble::core
