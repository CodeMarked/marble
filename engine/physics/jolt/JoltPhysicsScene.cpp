#include "physics/IPhysicsScene.hpp"

#include "core/MeshAssetV1PhysicsExtract.hpp"
#include "math/Geometry.hpp"
#include "math/Mat4.hpp"
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
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
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

/// Matches [`marble::math::Mat4::rotationY`] * rotationX * rotationZ composition order on column vectors.
[[nodiscard]] Quat gardenYprToJoltQuat(float yaw, float pitch, float roll) noexcept {
    Quat const qz = Quat::sRotation(Vec3::sAxisZ(), roll);
    Quat const qx = Quat::sRotation(Vec3::sAxisX(), pitch);
    Quat const qy = Quat::sRotation(Vec3::sAxisY(), yaw);
    return (qy * qx * qz).Normalized();
}

/// Minimal rotation taking `from` (unit) to `to` (unit); `from` is usually Jolt capsule +Y.
[[nodiscard]] Quat quatRotateFromTo(Vec3Arg from, Vec3Arg to) noexcept {
    Vec3 f = from;
    Vec3 t = to;
    if (f.IsNearZero(1.0e-12f) || t.IsNearZero(1.0e-12f)) {
        return Quat::sIdentity();
    }
    f = f.Normalized();
    t = t.Normalized();
    float const d = f.Dot(t);
    if (d > 1.0f - 1.0e-5f) {
        return Quat::sIdentity();
    }
    if (d < -1.0f + 1.0e-5f) {
        Vec3 axis = Vec3::sAxisX().Cross(f);
        if (axis.LengthSq() < 1.0e-8f) {
            axis = Vec3::sAxisZ().Cross(f);
        }
        return Quat::sRotation(axis.Normalized(), JPH_PI);
    }
    Vec3 const c = f.Cross(t);
    float const w = 1.0f + d;
    return Quat(c.GetX(), c.GetY(), c.GetZ(), w).Normalized();
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

    PhysicsBodyId addStaticSphere(PhysicsStaticSphereDesc const& desc) override {
        float const rad = std::max(desc.radius, 1.0e-4f);
        SphereShapeSettings sphere_settings(rad);
        sphere_settings.SetEmbedded();
        ShapeSettings::ShapeResult sr = sphere_settings.Create();
        if (sr.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = sr.Get();
        BodyCreationSettings body_settings(
            shape,
            toRVec3(desc.center),
            Quat::sIdentity(),
            EMotionType::Static,
            Layers::NON_MOVING);
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

    PhysicsBodyId addStaticOrientedBox(PhysicsStaticOrientedBoxDesc const& desc) override {
        Vec3 const half(
            (std::max)(std::abs(desc.halfExtents.x), 1.0e-4f),
            (std::max)(std::abs(desc.halfExtents.y), 1.0e-4f),
            (std::max)(std::abs(desc.halfExtents.z), 1.0e-4f));
        BoxShapeSettings box_settings(half);
        box_settings.SetEmbedded();
        ShapeSettings::ShapeResult shape_result = box_settings.Create();
        if (shape_result.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = shape_result.Get();
        Quat const q = gardenYprToJoltQuat(desc.yawRadians, desc.pitchRadians, desc.rollRadians);
        BodyCreationSettings body_settings(shape, toRVec3(desc.center), q, EMotionType::Static, Layers::NON_MOVING);
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

    PhysicsBodyId addStaticCapsule(PhysicsStaticCapsuleDesc const& desc) override {
        float const hh = std::max(desc.halfHeight, 1.0e-4f);
        float const rad = std::max(desc.radius, 1.0e-4f);
        CapsuleShapeSettings capsule_settings(hh, rad);
        capsule_settings.SetEmbedded();
        ShapeSettings::ShapeResult sr = capsule_settings.Create();
        if (sr.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = sr.Get();
        math::Mat4 const r = math::Mat4::rotationY(desc.yawRadians) * math::Mat4::rotationX(desc.pitchRadians) *
            math::Mat4::rotationZ(desc.rollRadians);
        math::Vec3 meshAxis{0.f, 1.f, 0.f};
        if (desc.intrinsicCylinderAxis == 1u) {
            meshAxis = {1.f, 0.f, 0.f};
        } else if (desc.intrinsicCylinderAxis == 2u) {
            meshAxis = {0.f, 0.f, 1.f};
        }
        math::Vec3 const wAxis = math::transformDirection(r, meshAxis);
        Quat const q = quatRotateFromTo(Vec3::sAxisY(), toVec3(wAxis));
        BodyCreationSettings body_settings(
            shape,
            toRVec3(desc.center),
            q,
            EMotionType::Static,
            Layers::NON_MOVING);
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

    PhysicsBodyId addStaticConvexHull(PhysicsStaticConvexHullDesc const& desc) override {
        std::size_t const n = desc.points.size();
        if (n == 0) {
            return kInvalidPhysicsBodyId;
        }
        int const cap = ConvexHullShape::cMaxPointsInHull;
        Array<Vec3> pts;
        if (static_cast<int>(n) <= cap) {
            pts.reserve(static_cast<int>(n));
            for (math::Vec3 const& p : desc.points) {
                pts.push_back(toVec3(p));
            }
        } else {
            pts.reserve(cap);
            for (int i = 0; i < cap; ++i) {
                std::size_t const idx =
                    n <= 1 ? 0 : (static_cast<std::size_t>(i) * (n - 1)) / (static_cast<std::size_t>(cap) - 1);
                pts.push_back(toVec3(desc.points[idx]));
            }
        }
        ConvexHullShapeSettings hull_settings(pts);
        hull_settings.SetEmbedded();
        ShapeSettings::ShapeResult const sr = hull_settings.Create();
        if (sr.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = sr.Get();
        Quat const q = gardenYprToJoltQuat(desc.yawRadians, desc.pitchRadians, desc.rollRadians);
        BodyCreationSettings body_settings(shape, toRVec3(desc.center), q, EMotionType::Static, Layers::NON_MOVING);
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

    PhysicsBodyId addStaticTriangleMesh(PhysicsStaticTriangleMeshDesc const& desc) override {
        if (desc.vertices.empty() || desc.indices.size() < 3u || desc.indices.size() % 3u != 0u) {
            return kInvalidPhysicsBodyId;
        }
        if (desc.indices.size() / 3u > marble::core::kMeshAssetV1PhysicsMaxTriangles) {
            return kInvalidPhysicsBodyId;
        }
        VertexList vl;
        vl.reserve(static_cast<int>(desc.vertices.size()));
        for (math::Vec3 const& v : desc.vertices) {
            Vec3 const vv = toVec3(v);
            vl.push_back(Float3(vv.GetX(), vv.GetY(), vv.GetZ()));
        }
        IndexedTriangleList tris;
        tris.reserve(static_cast<int>(desc.indices.size() / 3u));
        for (std::size_t t = 0; t < desc.indices.size(); t += 3u) {
            tris.push_back(IndexedTriangle(
                desc.indices[t],
                desc.indices[t + 1u],
                desc.indices[t + 2u],
                0u
            ));
        }
        MeshShapeSettings mesh_settings(std::move(vl), std::move(tris));
        mesh_settings.SetEmbedded();
        ShapeSettings::ShapeResult const sr = mesh_settings.Create();
        if (sr.HasError()) {
            return kInvalidPhysicsBodyId;
        }
        ShapeRefC shape = sr.Get();
        Quat const q = gardenYprToJoltQuat(desc.yawRadians, desc.pitchRadians, desc.rollRadians);
        BodyCreationSettings body_settings(shape, toRVec3(desc.center), q, EMotionType::Static, Layers::NON_MOVING);
        body_settings.mRestitution = desc.material.restitution;
        body_settings.mFriction = desc.material.friction;
        body_settings.mUserData = packFilter(desc.filter);
        body_settings.mEnhancedInternalEdgeRemoval = desc.enhancedInternalEdgeRemoval;
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
        bs.mEnhancedInternalEdgeRemoval = desc.enhancedInternalEdgeRemoval;
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        BodyID const id = iface.CreateAndAddBody(bs, EActivation::Activate);
        if (id.IsInvalid()) {
            return kInvalidPhysicsBodyId;
        }
        iface.SetLinearVelocity(id, toVec3(desc.linearVelocity));
        slots_.push_back(BodySlot{id, true, true});
        return static_cast<PhysicsBodyId>(slots_.size());
    }

    PhysicsBodyId addDynamicCapsule(PhysicsDynamicCapsuleDesc const& desc) override {
        CapsuleShapeSettings capsule_settings(desc.halfHeight, desc.radius);
        capsule_settings.SetEmbedded();
        ShapeSettings::ShapeResult sr = capsule_settings.Create();
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
                // Host-authoritative games push velocities from gameplay each tick; waking ensures impulses
                // and synced velocities apply even if Jolt put the body to sleep (e.g. after penetration).
                iface.ActivateBody(j);
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
                Quat const q = iface.GetRotation(j);
                iface.SetPositionAndRotationWhenChanged(j, toRVec3(c), q, EActivation::Activate);
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

    void applyLinearImpulse(PhysicsBodyId id, math::Vec3 impulse) override {
        BodyID const j = joltId(id);
        if (j.IsInvalid()) {
            return;
        }
        impl_->physics_system.GetBodyInterface().AddImpulse(j, toVec3(impulse));
    }

    void setBodyLinearVelocity(PhysicsBodyId id, math::Vec3 linearVelocity) override {
        BodyID const j = joltId(id);
        if (j.IsInvalid()) {
            return;
        }
        impl_->physics_system.GetBodyInterface().SetLinearVelocity(j, toVec3(linearVelocity));
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

    void setBodyYawAboutY(PhysicsBodyId id, float yawRadians) noexcept override {
        BodyID const j = joltId(id);
        if (j.IsInvalid()) {
            return;
        }
        BodyInterface& iface = impl_->physics_system.GetBodyInterface();
        RVec3 const p = iface.GetCenterOfMassPosition(j);
        Quat const q = Quat::sRotation(Vec3::sAxisY(), yawRadians);
        iface.SetPositionAndRotationWhenChanged(j, p, q, EActivation::Activate);
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
