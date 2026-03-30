#pragma once

#include "math/Mat4.hpp"
#include "physics/RigidBodyDynamics.hpp"

#include <cstddef>
#include <cstdint>

namespace marble::physics {

/// Tunables for a **game-owned** physics step. `SimplePhysicsWorld` applies only `gravity` today;
/// `enableContinuousCollision` and `maxSubSteps` reserve policy for middleware / advanced pipelines
/// (book §13.6) without changing baseline behavior until wired.
struct PhysicsWorldSettings {
    math::Vec3 gravity{0.f, -9.81f, 0.f};
    bool enableContinuousCollision{};
    std::uint8_t maxSubSteps{1u};
};

/// Minimal stand-in for a third-party physics `World` (book §13.5): uniform gravity + per-body
/// [`integrateSemiImplicitEuler`](RigidBodyDynamics.hpp). Replace `step` internals with SDK calls when
/// linking middleware; keep `PhysicsWorldSettings` as the engine-side configuration seam.
class SimplePhysicsWorld {
public:
    void setSettings(PhysicsWorldSettings settings) noexcept {
        settings_ = settings;
    }

    [[nodiscard]] PhysicsWorldSettings settings() const noexcept {
        return settings_;
    }

    void step(float deltaSeconds, RigidBodyKinematics* bodies, std::size_t bodyCount) noexcept {
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
