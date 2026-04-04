// Compile/link smoke test for Jolt (optional; MARBLE_WITH_JOLT=ON). Derived from Jolt HelloWorld (CC0).

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <thread>

JPH_SUPPRESS_WARNINGS

using namespace JPH;

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

static void TraceImpl(char const* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    (void)std::vprintf(inFMT, list);
    va_end(list);
    (void)std::printf("\n");
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(char const* inExpression, char const* inMessage, char const* inFile, uint inLine) {
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

int main() {
    RegisterDefaultAllocator();
    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

    Factory::sInstance = new Factory();
    RegisterTypes();

    TempAllocatorMalloc temp_allocator;
    unsigned const workerThreads = (std::max)(1u, static_cast<unsigned>(std::thread::hardware_concurrency()) - 1u);
    JobSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, workerThreads);

    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;

    PhysicsSystem physics_system;
    physics_system.Init(
        1024,
        0,
        1024,
        1024,
        broad_phase_layer_interface,
        object_vs_broadphase_layer_filter,
        object_vs_object_layer_filter
    );

    constexpr float cDeltaTime = 1.0f / 60.0f;
    constexpr int cCollisionSteps = 1;
    physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
    physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);

    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    return 0;
}
