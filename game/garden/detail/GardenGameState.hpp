#pragma once

#include "garden/GardenGame.hpp"

#include "garden/GardenAuthorityTick.hpp"
#include "garden/GardenRemotePresentation.hpp"
#include "core/Engine.hpp"
#include "core/ResourceManager.hpp"
#include "input/HidMapping.hpp"
#include "input/PlatformKeyboardBridge.hpp"
#include "garden/GardenSimulation.hpp"
#include "input/PlatformGamepadBridge.hpp"
#include "input/PlatformKeyboardBridge.hpp"
#include "math/Geometry.hpp"
#include "math/Mat4.hpp"
#include "math/Vec3.hpp"
#include "physics/IPhysicsScene.hpp"
#include "physics/MiddlewarePhysicsTypes.hpp"
#include "physics/PhysicsIntegration.hpp"
#include "physics/RigidBodyDynamics.hpp"
#include "render/DrawFlags.hpp"
#include "render/IRenderBackend.hpp"
#include "render/MaterialId.hpp"
#include "render/RenderTypes.hpp"
#include "render/vulkan/VulkanRhi.hpp"

#include "gameplay/AuthoritativeSession.hpp"
#include "gameplay/ClientSession.hpp"
#include "gameplay/MultiplayerSessionEnvelope.hpp"
#include "gameplay/MultiplayerWireFormat.hpp"
#include "gameplay/OnlineMultiplayerFoundation.hpp"
#include "gameplay/SimulationIsland.hpp"
#include "gameplay/SnapshotInterpolator.hpp"
#include "gameplay/UdpGameTransport.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace marble::garden_app {

enum class PausePanel : std::uint8_t {
    Main,
    Options,
};

/// Must match `garden_server` default `SessionConfig::simulationHz`.
inline constexpr float kGardenRemoteServerSimulationHz = 60.f;
/// Beyond this error vs last authority sample, snap local display pose to authority (recovery only; no per-frame pull).
inline constexpr float kGardenRemoteReconcileSnapThresholdM = 0.55f;
inline constexpr std::size_t kRemoteAuthMarbleCapacity = 8u;

inline constexpr std::uint32_t kGardenRemoteInterpDelayMinTicks = 2u;
inline constexpr std::uint32_t kGardenRemoteInterpDelayMaxTicks = 14u;
inline constexpr std::uint32_t kGardenRemoteMaxExtrapolationTicks = 2u;
inline constexpr float kGardenRemoteVelocityJumpBlendMps = 6.f;
inline constexpr float kGardenRemoteVelocityJumpExtrapMps = 6.f;
inline constexpr float kGardenRemoteLatestClampMinSpeedMps = 1.5f;
inline constexpr float kGardenRemoteLatestClampDvMps = 5.f;
/// RTT multiplier when deriving interp delay ticks from `estimatedRttSeconds() * hz` (see env `GARDEN_REMOTE_INTERP_RTT_SCALE`).
inline constexpr float kGardenRemoteInterpRttScale = 0.5f;
/// Max render-delay ticks to add/remove per frame when smoothing toward RTT-derived delay.
inline constexpr std::uint32_t kGardenRemoteInterpDelaySmoothMaxTicks = 1u;

inline constexpr marble::input::LogicalActionId kActionPauseMenu = 1;
inline constexpr marble::input::ActionContextEntry kGameplayContext[] = {
    {kActionPauseMenu, marble::input::deviceMask(marble::input::LogicalDevice::Player), false},
};

using marble::math::Aabb;
using marble::math::Mat4;
using marble::math::Vec3;

struct GardenGame::State final {
    static constexpr std::size_t kPropMeshCount = static_cast<std::size_t>(marble::garden::GardenPropMesh::Count);

    marble::core::Engine& engine;
    marble::render::VulkanRhi rhi{};
    std::uint32_t meshTerrain = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t meshCube = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t meshSphere = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, kPropMeshCount> meshProps{};

    marble::garden::GardenLayout layout{};
    static constexpr std::size_t kAuthMarbleCap = marble::garden::kMaxGardenAuthorityMarbles;
    std::array<marble::physics::RigidBodyKinematics, kAuthMarbleCap> marbles{};
    std::size_t marbleCount_{2u};
    marble::physics::SimplePhysicsWorld physics{};
    std::unique_ptr<marble::physics::IPhysicsScene> physicsScene_{marble::physics::createJoltPhysicsScene()};
    std::array<marble::physics::PhysicsBodyId, kAuthMarbleCap> marbleBodyIds_{};

    marble::gameplay::SimulationIsland island_{};
    marble::gameplay::SessionConfig sessionConfig_{};
    marble::gameplay::AuthorityRoster<4> roster_{};
    GardenSessionKind sessionKind_ = GardenSessionKind::Offline;
    RemoteClientParams clientParams_{};

    std::unique_ptr<marble::gameplay::UdpGameTransport> hostTransport_{};
    marble::gameplay::AuthoritativeSession<> hostSession_{};
    bool hostListenInitialized_{false};
    std::uint16_t hostListenPort_{27778u};
    marble::physics::PhysicsWorldSettings hostAuthorityWorld_{};
    std::array<float, kAuthMarbleCap> hostPeerJumpHoldSec_{};
    std::array<bool, kAuthMarbleCap> hostPeerJumpWasHeld_{};
    std::uint32_t hostInputTickCounter_{};
    /// Wall-time accumulator for listen-host authority fixed steps (matches dedicated `garden_server` cadence).
    float hostAuthorityFixedAccum_{};
    /// Latest local wish for synthetic peer 2 input (submitted each fixed authority step).
    float hostListenWishX_{};
    float hostListenWishZ_{};
    bool hostListenJumpHeld_{};

    std::unique_ptr<marble::gameplay::UdpGameTransport> clientTransport_{};
    marble::gameplay::ClientSession<> clientSession_{};
    marble::gameplay::SnapshotInterpolator<> snapInterp_{};
    bool clientConnected_{};
    std::optional<std::uint32_t> remoteLastConsumedSnapshotServerTick_{};
    std::array<Vec3, kRemoteAuthMarbleCapacity> remoteAuthMarblePos_{};
    std::array<bool, kRemoteAuthMarbleCapacity> remoteAuthMarbleValid_{};
    std::array<float, kRemoteAuthMarbleCapacity> remoteAuthMarbleYaw_{};
    std::array<bool, kRemoteAuthMarbleCapacity> remoteAuthMarbleYawValid_{};
    std::array<float, kRemoteAuthMarbleCapacity> remoteDisplayYaw_{};
    /// Last [`PhysicsSimulationTier`] from snapshots per marble slot (prototype net-sim tier display + client policy).
    std::array<marble::gameplay::PhysicsSimulationTier, kRemoteAuthMarbleCapacity> remoteMarbleReplTier_{};
    std::uint32_t lastAckedServerSimTick_{};

    std::optional<std::chrono::steady_clock::time_point> lastDesyncLogTime_{};
    std::optional<std::chrono::steady_clock::time_point> lastInterpLogTime_{};
    std::uint64_t desyncPosSnapCount_{};

    float camYaw = 0.7f;
    float camDist = 14.f;
    float camHeight = 3.6f;
    float camYawTarget = 0.7f;
    float camDistTarget = 14.f;
    float camHeightTarget = 3.6f;

    float strokeVelEmaX = 0.f;
    float strokeVelEmaZ = 0.f;
    bool strokeEmaValid = false;
    float padStrokeVelEmaX = 0.f;
    float padStrokeVelEmaZ = 0.f;
    bool padStrokeEmaValid = false;

    bool mouseWasDown = false;
    bool rightMouseWasDown = false;
    bool mouseFlickArmed = false;
    float strokePlaneY = 0.f;
    bool hasPlaneStroke = false;
    Vec3 pStrokeStart{};
    Vec3 pStrokeLast{};
    Vec3 pStrokePrev{};

    bool padWasDown = false;
    bool padFlickArmed = false;
    float padStrokePlaneY = 0.f;
    Vec3 padPStart{};
    Vec3 padPLast{};
    Vec3 padPPrev{};

    float jumpChargeSec_ = 0.f;
    bool jumpWasHeld_ = false;

    float pendingMoveX_{};
    float pendingMoveZ_{};
    std::uint8_t pendingButtons_{};
    std::uint32_t inputTickCounter_{};

    GardenRemoteNetTuning remoteNetTuning_{};
    float remoteClientPredictAccum_{};
    /// 0 = uninitialized; ramps RTT-derived interp delay by at most one tick per frame to reduce jitter.
    std::uint32_t remoteInterpDelaySmoothed_{};

    std::vector<marble::render::MeshDrawInstance> drawScratch{};
    marble::input::InputRemapTable inputRemap_{};
    marble::input::ActionPolicy<256> inputPolicy_{};
    marble::input::AbstractControlArray controlScratch_{};
    marble::core::BinaryResourceManager<64> assetRegistry_{};
    bool assetRegistryPrimed_{false};

    bool paused = false;
    PausePanel pausePanel = PausePanel::Main;
    bool pauseBackWasDown = false;
    bool menuRWasDown = false;
    bool menuTWasDown = false;
    bool menuOWasDown = false;
    bool menuQWasDown = false;
    bool padMenuAWasDown = false;
    bool padMenuXWasDown = false;
    bool padMenuBWasDown = false;
    bool padMenuYWasDown = false;

    static constexpr std::uint32_t kLayoutSeed = 0xC001D00u;

    [[nodiscard]] std::uint32_t layoutSeedForCurrentSession() const noexcept {
        return sessionKind_ == GardenSessionKind::RemoteClient ? clientParams_.layoutSeed : kLayoutSeed;
    }

    void rebuildPhysicsFromLayout() noexcept;
    void rebuildListenHostDynamics() noexcept;
    void rebuildListenHostPhysicsScene() noexcept;
    void initListenHostNetworking() noexcept;
    void fallbackFromListenHostToOffline() noexcept;

    explicit State(marble::core::Engine& e, GardenSessionKind session, RemoteClientParams params = {});

    void initClientNetworking() noexcept;
    void primeAssetRegistryOnce();
    void restartGarden() noexcept;
    [[nodiscard]] std::size_t localViewMarbleIndex() const noexcept;
    void tickRemoteClient(float dt) noexcept;

    void step(marble::core::Engine::FrameContext const& ctx);
    void renderFrame(marble::core::Engine::FrameContext const& ctx);
};

} // namespace marble::garden_app
