#include "physics/IPhysicsScene.hpp"

#include "math/Geometry.hpp"
#include "math/Vec3.hpp"
#include "physics/CollisionMiddleware.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionQuality.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <Jolt/Math/Mat44.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

JPH_SUPPRESS_WARNINGS

using namespace JPH;

namespace marble::physics {
namespace {

void traceJolt(char const* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    (void)std::vprintf(inFMT, list);
    va_end(list);
    (void)std::printf("\n");
}

#ifdef JPH_ENABLE_ASSERTS
bool assertFailedJolt(char const* inExpression, char const* inMessage, char const* inFile, uint inLine) {
    (void)std::fprintf(
        stderr,
        "%s:%u: (%s) %s\n",
        inFile,
        static_cast<unsigned>(inLine),
        inExpression,
        inMessage != nullptr ? inMessage : ""
    );
    return true;
}
#endif

void ensureJoltGlobals() {
    static std::once_flag once;
    std::call_once(once, []() {
        RegisterDefaultAllocator();
        Trace = traceJolt;
        JPH_IF_ENABLE_ASSERTS(AssertFailed = assertFailedJolt;)
        Factory::sInstance = new Factory();
        RegisterTypes();
    });
}

namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0;
static constexpr ObjectLayer MOVING = 1;
static constexpr ObjectLayer NUM_LAYERS = 2;
} // namespace Layers

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
        switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

namespace BroadPhaseLayers {
static BroadPhaseLayer const NON_MOVING(0);
static BroadPhaseLayer const MOVING(1);
static constexpr uint NUM_LAYERS(2);
} // namespace BroadPhaseLayers

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS]{};
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

[[nodiscard]] RVec3 toRVec3(math::Vec3 v) noexcept {
    return RVec3(static_cast<Real>(v.x), static_cast<Real>(v.y), static_cast<Real>(v.z));
}

[[nodiscard]] Vec3 toVec3(math::Vec3 v) noexcept {
    return Vec3(v.x, v.y, v.z);
}

[[nodiscard]] math::Vec3 fromVec3(Vec3Arg v) noexcept {
    return math::Vec3{v.GetX(), v.GetY(), v.GetZ()};
}

[[nodiscard]] math::Vec3 fromRVec3(RVec3Arg v) noexcept {
    return math::Vec3{
        static_cast<float>(v.GetX()),
        static_cast<float>(v.GetY()),
        static_cast<float>(v.GetZ())
    };
}

[[nodiscard]] math::Mat4 fromJoltMat44(Mat44Arg jm) noexcept {
    math::Mat4 u{};
    for (uint c = 0; c < 4; ++c) {
        Vec4 const col = jm.GetColumn4(c);
        u.m[c * 4 + 0] = col.GetX();
        u.m[c * 4 + 1] = col.GetY();
        u.m[c * 4 + 2] = col.GetZ();
        u.m[c * 4 + 3] = col.GetW();
    }
    return u;
}

[[nodiscard]] constexpr std::uint64_t packFilter(marble::physics::CollisionFilter f) noexcept {
    return (static_cast<std::uint64_t>(f.membershipLayers) << 32) |
            static_cast<std::uint64_t>(f.collideAgainstMask);
}

[[nodiscard]] constexpr marble::physics::CollisionFilter unpackFilter(std::uint64_t ud) noexcept {
    return {
        static_cast<std::uint32_t>(ud >> 32),
        static_cast<std::uint32_t>(ud & 0xFFFFFFFFu)
    };
}

class CollisionFilterListener final : public ContactListener {
public:
    ValidateResult OnContactValidate(
        Body const& inBody1,
        Body const& inBody2,
        [[maybe_unused]] RVec3Arg inBaseOffset,
        [[maybe_unused]] CollideShapeResult const& inCollisionResult
    ) override {
        auto const f1 = unpackFilter(inBody1.GetUserData());
        auto const f2 = unpackFilter(inBody2.GetUserData());
        if (!marble::physics::filtersAllow(f1, f2)) {
            return ValidateResult::RejectAllContactsForThisBodyPair;
        }
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }
};

struct BodySlot {
    BodyID jolt{};
    bool in_use{};
    bool is_dynamic{};
};

void applyCylindricalClampToCenter(math::Vec3& center, PhysicsCylindricalXZClamp const& clamp) noexcept {
    float const xz = std::sqrt(center.x * center.x + center.z * center.z);
    if (xz > clamp.maxHorizontalRadiusFromYAxis && xz > 1e-6f) {
        float const s = clamp.maxHorizontalRadiusFromYAxis / xz;
        center.x *= s;
        center.z *= s;
    }
    if (center.y < clamp.minCenterY) {
        center.y = clamp.minCenterY;
    }
}

class JoltPhysicsScene final : public IPhysicsScene {
public:
    JoltPhysicsScene() {
        ensureJoltGlobals();
        impl_ = std::make_unique<Impl>();
    }

    ~JoltPhysicsScene() override = default;

    void clear() override {
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        for (BodySlot& s : slots_) {
            if (s.in_use) {
                destroyBody(iface, s.jolt);
                s.jolt = BodyID();
                s.in_use = false;
            }
        }
        slots_.clear();
    }

    PhysicsBodyId addStaticBox(PhysicsStaticBoxDesc const& desc) override {
        math::Aabb const& b = desc.bounds;
        Vec3 const c = toVec3((b.min + b.max) * 0.5f);
        Vec3 const half = toVec3((b.max - b.min) * 0.5f);
        BoxShapeSettings box_settings(Vec3(
            (std::max)(half.GetX(), 1.0e-4f),
            (std::max)(half.GetY(), 1.0e-4f),
            (std::max)(half.GetZ(), 1.0e-4f)
        ));
        box_settings.SetEmbedded();
        ShapeSettings::ShapeResult shape_result = box_settings.Create();
        if (shape_result.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = shape_result.Get();
        BodyCreationSettings body_settings(shape, RVec3(c), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
        body_settings.mRestitution = desc.material.restitution;
        body_settings.mFriction = desc.material.friction;
        body_settings.mUserData = packFilter(desc.filter);
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        Body* body = iface.CreateBody(body_settings);
        if (body == nullptr) {
            return kInvalidPhysicsBodyId;
        }
        iface.AddBody(body->GetID(), EActivation::DontActivate);
        slots_.push_back(BodySlot{body->GetID(), true});
        return static_cast<PhysicsBodyId>(slots_.size());
    }

    PhysicsBodyId addStaticHeightField(PhysicsStaticHeightFieldDesc const& desc) override {
        if (desc.sampleCount < 2u ||
            desc.heights.size() != static_cast<std::size_t>(desc.sampleCount) * static_cast<std::size_t>(desc.sampleCount)) {
            return kInvalidPhysicsBodyId;
        }
        HeightFieldShapeSettings hf_settings(
            desc.heights.data(),
            RVec3(static_cast<Real>(desc.offset.x), static_cast<Real>(desc.offset.y), static_cast<Real>(desc.offset.z)),
            RVec3(static_cast<Real>(desc.scale.x), static_cast<Real>(desc.scale.y), static_cast<Real>(desc.scale.z)),
            desc.sampleCount
        );
        ShapeSettings::ShapeResult shape_result = hf_settings.Create();
        if (shape_result.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = shape_result.Get();
        BodyCreationSettings body_settings(shape, RVec3::sZero(), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
        body_settings.mRestitution = desc.material.restitution;
        body_settings.mFriction = desc.material.friction;
        body_settings.mUserData = packFilter(desc.filter);
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        Body* body = iface.CreateBody(body_settings);
        if (body == nullptr) {
            return kInvalidPhysicsBodyId;
        }
        iface.AddBody(body->GetID(), EActivation::DontActivate);
        slots_.push_back(BodySlot{body->GetID(), true});
        return static_cast<PhysicsBodyId>(slots_.size());
    }

    PhysicsBodyId addDynamicSphere(PhysicsDynamicSphereDesc const& desc) override {
        SphereShapeSettings sphere_settings(desc.radius);
        sphere_settings.SetEmbedded();
        ShapeSettings::ShapeResult sr = sphere_settings.Create();
        if (sr.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = sr.Get();
        BodyCreationSettings bs(
            shape,
            toRVec3(desc.center),
            Quat::sIdentity(),
            EMotionType::Dynamic,
            Layers::MOVING
        );
        bs.mRestitution = desc.material.restitution;
        bs.mFriction = desc.material.friction;
        bs.mLinearDamping = desc.material.linearDamping;
        bs.mAngularDamping = desc.material.angularDamping;
        bs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        bs.mMassPropertiesOverride.mMass =
            desc.invMass > 0.f ? (1.f / desc.invMass) : 1.f;
        bs.mMotionQuality = EMotionQuality::LinearCast;
        bs.mAllowSleeping = sleepingEnabled_;
        bs.mUserData = packFilter(desc.filter);
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        BodyID const id = iface.CreateAndAddBody(bs, EActivation::Activate);
        if (id.IsInvalid()) {
            return kInvalidPhysicsBodyId;
        }
        iface.SetLinearVelocity(id, toVec3(desc.linearVelocity));
        slots_.push_back(BodySlot{id, true, true});
        return static_cast<PhysicsBodyId>(slots_.size());
    }

    void removeBody(PhysicsBodyId id) override {
        BodyID j = joltId(id);
        if (j.IsInvalid()) {
            return;
        }
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        destroyBody(iface, j);
        slots_[static_cast<std::size_t>(id) - 1U].jolt = BodyID();
        slots_[static_cast<std::size_t>(id) - 1U].in_use = false;
    }

    void optimizeBroadPhase() override {
        impl_->physics_system.OptimizeBroadPhase();
    }

    void syncHostVelocitiesBeforeStep(
        std::span<PhysicsBodyId const> ids,
        RigidBodyKinematics const* hostKinematics,
        std::size_t count
    ) override {
        if (hostKinematics == nullptr || ids.size() != count) {
            return;
        }
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        for (std::size_t i = 0; i < count; ++i) {
            BodyID const j = joltId(ids[i]);
            if (!j.IsInvalid()) {
                iface.SetLinearVelocity(j, toVec3(hostKinematics[i].linearVelocity));
            }
        }
    }

    void step(float deltaSeconds, PhysicsWorldSettings const& settings, PhysicsStepOptions const& options) override {
        if (deltaSeconds <= 0.f) {
            return;
        }
        impl_->physics_system.SetGravity(toVec3(settings.gravity));

        PhysicsSettings ps = impl_->physics_system.GetPhysicsSettings();
        if (settings.velocitySolverIterations != 0) {
            ps.mNumVelocitySteps = static_cast<uint>(settings.velocitySolverIterations);
        }
        if (settings.positionSolverIterations != 0) {
            ps.mNumPositionSteps = static_cast<uint>(settings.positionSolverIterations);
        }
        if (settings.enableContinuousCollision) {
            ps.mSpeculativeContactDistance = 0.055f;
            ps.mLinearCastThreshold = 0.42f;
        } else {
            ps.mSpeculativeContactDistance = 0.02f;
            ps.mLinearCastThreshold = 0.75f;
        }
        if (settings.enableSleeping != sleepingEnabled_) {
            sleepingEnabled_ = settings.enableSleeping;
            BodyLockInterface const& lockIf = impl_->physics_system.GetBodyLockInterface();
            for (BodySlot const& s : slots_) {
                if (s.in_use && s.is_dynamic && !s.jolt.IsInvalid()) {
                    BodyLockWrite lock(lockIf, s.jolt);
                    if (lock.Succeeded()) {
                        lock.GetBody().SetAllowSleeping(sleepingEnabled_);
                    }
                }
            }
            if (!sleepingEnabled_) {
                BodyInterface& bodyIf = impl_->physics_system.GetBodyInterface();
                for (BodySlot const& s : slots_) {
                    if (s.in_use && s.is_dynamic && !s.jolt.IsInvalid()) {
                        bodyIf.ActivateBody(s.jolt);
                    }
                }
            }
        }
        impl_->physics_system.SetPhysicsSettings(ps);

        unsigned const collisionSteps =
            settings.maxSubSteps == 0 ? 1u : static_cast<unsigned>(settings.maxSubSteps);
        impl_->physics_system.Update(
            deltaSeconds,
            static_cast<int>(collisionSteps),
            &impl_->temp_allocator,
            &impl_->job_system
        );

        if (options.postStepCylindricalClamp != nullptr && !options.clampBodyIds.empty()) {
            BodyInterface& iface = impl_->physics_system.GetBodyInterface();
            PhysicsCylindricalXZClamp const& clamp = *options.postStepCylindricalClamp;
            for (PhysicsBodyId pid : options.clampBodyIds) {
                BodyID const j = joltId(pid);
                if (j.IsInvalid()) {
                    continue;
                }
                math::Vec3 c = fromRVec3(iface.GetCenterOfMassPosition(j));
                applyCylindricalClampToCenter(c, clamp);
                iface.SetPositionAndRotationWhenChanged(j, toRVec3(c), Quat::sIdentity(), EActivation::Activate);
                iface.SetLinearVelocity(j, iface.GetLinearVelocity(j));
            }
        }
    }

    void readBackKinematics(std::span<PhysicsBodyId const> ids, RigidBodyKinematics* out, std::size_t count)
        const override {
        if (out == nullptr || ids.size() != count) {
            return;
        }
        BodyInterface const& iface = impl_->physics_system.GetBodyInterface();
        for (std::size_t i = 0; i < count; ++i) {
            BodyID const j = joltId(ids[i]);
            if (j.IsInvalid()) {
                continue;
            }
            out[i].position = fromRVec3(iface.GetCenterOfMassPosition(j));
            out[i].linearVelocity = fromVec3(iface.GetLinearVelocity(j));
        }
    }

    void setBodyCenterAndLinearVelocity(PhysicsBodyId id, math::Vec3 center, math::Vec3 linearVelocity) override {
        BodyID const j = joltId(id);
        if (j.IsInvalid()) {
            return;
        }
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        iface.SetPositionAndRotationWhenChanged(j, toRVec3(center), Quat::sIdentity(), EActivation::Activate);
        iface.SetLinearVelocity(j, toVec3(linearVelocity));
        iface.SetAngularVelocity(j, Vec3::sZero());
    }

    void activateBody(PhysicsBodyId id) override {
        BodyID const j = joltId(id);
        if (j.IsInvalid()) {
            return;
        }
        impl_->physics_system.GetBodyInterface().ActivateBody(j);
    }

    [[nodiscard]] math::Mat4 bodyWorldMatrix(PhysicsBodyId id) const override {
        BodyID const j = joltId(id);
        if (j.IsInvalid()) {
            return math::Mat4::identity();
        }
        BodyInterface const& iface = impl_->physics_system.GetBodyInterface();
        RMat44 const rjm = iface.GetCenterOfMassTransform(j);
        Mat44 const jm = rjm.ToMat44();
        return fromJoltMat44(jm);
    }

private:
    [[nodiscard]] BodyID joltId(PhysicsBodyId id) const noexcept {
        if (id == kInvalidPhysicsBodyId || static_cast<std::size_t>(id) > slots_.size()) {
            return BodyID();
        }
        BodySlot const& s = slots_[static_cast<std::size_t>(id) - 1U];
        if (!s.in_use || s.jolt.IsInvalid()) {
            return BodyID();
        }
        return s.jolt;
    }

    static void destroyBody(BodyInterface& iface, BodyID id) {
        if (!id.IsInvalid()) {
            if (iface.IsAdded(id)) {
                iface.RemoveBody(id);
            }
            iface.DestroyBody(id);
        }
    }

    struct Impl {
        TempAllocatorMalloc temp_allocator{};
        ObjectLayerPairFilterImpl object_pair_filter{};
        BPLayerInterfaceImpl broad_phase_layer_interface{};
        ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter{};
        CollisionFilterListener contact_listener{};
        JobSystemThreadPool job_system;
        PhysicsSystem physics_system{};

        Impl()
            : job_system(
                  cMaxPhysicsJobs,
                  cMaxPhysicsBarriers,
                  (std::max)(1u, static_cast<unsigned>(std::thread::hardware_concurrency()) - 1u)
              ) {
            uint const maxBodies = 65536;
            uint const maxPairs = 65536;
            uint const maxContacts = 20480;
            physics_system.Init(
                maxBodies,
                0,
                maxPairs,
                maxContacts,
                broad_phase_layer_interface,
                object_vs_broadphase_layer_filter,
                object_pair_filter
            );
            physics_system.SetContactListener(&contact_listener);
        }
    };

    std::unique_ptr<Impl> impl_;
    std::vector<BodySlot> slots_;
    bool sleepingEnabled_{};
};

} // namespace

std::unique_ptr<IPhysicsScene> createJoltPhysicsScene() {
    return std::make_unique<JoltPhysicsScene>();
}

} // namespace marble::physics
