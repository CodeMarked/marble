#pragma once

#include "math/Mat4.hpp"
#include "physics/PhysicsWorld.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <cstddef>
#include <cstdint>

namespace marble::physics {

/// Minimal stand-in for a third-party physics `World` (book §13.5): uniform gravity + per-body
/// [`integrateSemiImplicitEuler`](RigidBodyDynamics.hpp). Replace `step` internals with SDK calls when
/// linking middleware; keep [`PhysicsWorldSettings`](PhysicsWorld.hpp) as the engine-side configuration seam.
class SimplePhysicsWorld final : public IPhysicsWorld {
public:
    void setSettings(PhysicsWorldSettings settings) noexcept override {
        settings_ = settings;
    }

    [[nodiscard]] PhysicsWorldSettings settings() const noexcept override {
        return settings_;
    }

    void step(float deltaSeconds, RigidBodyKinematics* bodies, std::size_t bodyCount) noexcept override {
        if (deltaSeconds <= 0.f || bodies == nullptr) {
            return;
        }
        (void)settings_.enableContinuousCollision;
        const unsigned subSteps = settings_.maxSubSteps == 0 ? 1u : static_cast<unsigned>(settings_.maxSubSteps);
        const float h = deltaSeconds / static_cast<float>(subSteps);
        for (unsigned s = 0; s < subSteps; ++s) {
            for (std::size_t i = 0; i < bodyCount; ++i) {
                integrateSemiImplicitEuler(bodies[i], settings_.gravity, h);
            }
        }
    }

private:
    PhysicsWorldSettings settings_{};
};

/// Gameplay/render sync helper: translation-only matrix from integrated position.
[[nodiscard]] inline math::Mat4 rigidBodyTranslationMatrix(RigidBodyKinematics const& body) noexcept {
    return math::Mat4::translation(body.position);
}

} // namespace marble::physics
