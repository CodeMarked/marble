#include "garden/detail/GardenGameState.hpp"
#include "garden/GardenGameHelpers.hpp"
#include "garden/GardenMarblePlayer.hpp"
#include "garden/GardenSimulation.hpp"
#include "platform/window/Window.hpp"
#include "shared/PauseMenuInput.hpp"

#include <cstdio>

namespace marble::garden_app {

using namespace marble::gameplay;
using namespace marble::physics;
using namespace marble::garden;
using namespace marble::input;
using namespace marble::platform;
using namespace detail;

void GardenGame::State::step(marble::core::Engine::FrameContext const& ctx) {
    primeAssetRegistryOnce();
    float const h = static_cast<float>(ctx.deltaSeconds);
    constexpr LogicalDevice kPlayer = LogicalDevice::Player;

    if (auto* w = engine.window()) {
        sampleKeyboardIntoAbstractControls(*w, controlScratch_);
        mergeAllConnectedGamepadsIntoAbstractControls(controlScratch_);

        bool const pauseHeld = actionScalar(kActionPauseMenu, controlScratch_, inputRemap_, inputPolicy_, kPlayer) >=
                0.5f ||
            w->isKeyDown(Key::Escape);
        if (marble::game_shared::pauseMenuToggleEdge(pauseHeld, pauseBackWasDown)) {
            paused = !paused;
            pausePanel = PausePanel::Main;
        }

        if (paused) {
            if (!anyStandardGamepadPresent() && pausePanel == PausePanel::Options) {
                pausePanel = PausePanel::Main;
            }
            bool const rDown = w->isKeyDown(Key::R);
            if (rDown && !menuRWasDown) {
                restartGarden();
            }
            bool const oDown = w->isKeyDown(Key::O);
            if (anyStandardGamepadPresent() && oDown && !menuOWasDown) {
                pausePanel = (pausePanel == PausePanel::Options) ? PausePanel::Main : PausePanel::Options;
            }
            menuRWasDown = rDown;
            menuOWasDown = oDown;

            std::size_t const iPadA = static_cast<std::size_t>(AbstractControl::RPadDown);
            std::size_t const iPadX = static_cast<std::size_t>(AbstractControl::RPadLeft);
            std::size_t const iPadB = static_cast<std::size_t>(AbstractControl::RPadRight);
            std::size_t const iPadY = static_cast<std::size_t>(AbstractControl::RPadUp);
            bool const qDown = w->isKeyDown(Key::Q);
            bool const aDown = controlScratch_[iPadA] >= 0.5f;
            bool const xDown = controlScratch_[iPadX] >= 0.5f;
            bool const bDown = controlScratch_[iPadB] >= 0.5f;
            bool const yDown = controlScratch_[iPadY] >= 0.5f;
            if (marble::game_shared::pauseMenuWantsReturnToLauncher(qDown, bDown, menuQWasDown, padMenuBWasDown)) {
                engine.requestEndRun();
            }
            if (aDown && !padMenuAWasDown) {
                paused = false;
                pausePanel = PausePanel::Main;
            }
            if (xDown && !padMenuXWasDown) {
                restartGarden();
            }
            if (anyStandardGamepadPresent() && yDown && !padMenuYWasDown) {
                pausePanel = (pausePanel == PausePanel::Options) ? PausePanel::Main : PausePanel::Options;
            }
            padMenuAWasDown = aDown;
            padMenuXWasDown = xDown;
            padMenuBWasDown = bDown;
            padMenuYWasDown = yDown;

            bool const mouseDown = w->isMouseButtonDown(MouseButton::Left);
            bool const rightDown = w->isMouseButtonDown(MouseButton::Right);
            std::size_t const iRbPause = static_cast<std::size_t>(AbstractControl::RShoulder);
            bool const padRbDown = controlScratch_[iRbPause] >= 0.5f;
            mouseWasDown = mouseDown;
            rightMouseWasDown = rightDown;
            padWasDown = padRbDown;
        } else {
            menuRWasDown = w->isKeyDown(Key::R);
            menuTWasDown = w->isKeyDown(Key::T);
            menuOWasDown = w->isKeyDown(Key::O);
            padMenuAWasDown = controlScratch_[static_cast<std::size_t>(AbstractControl::RPadDown)] >= 0.5f;
            padMenuXWasDown = controlScratch_[static_cast<std::size_t>(AbstractControl::RPadLeft)] >= 0.5f;
            padMenuYWasDown = controlScratch_[static_cast<std::size_t>(AbstractControl::RPadUp)] >= 0.5f;
            marble::game_shared::syncPauseMenuReturnEdgeState(
                w->isKeyDown(Key::Q),
                controlScratch_[static_cast<std::size_t>(AbstractControl::RPadRight)] >= 0.5f,
                menuQWasDown,
                padMenuBWasDown);

        std::size_t const iLt = static_cast<std::size_t>(AbstractControl::LTrigger);
        std::size_t const iRt = static_cast<std::size_t>(AbstractControl::RTrigger);
        std::size_t const iRx = static_cast<std::size_t>(AbstractControl::RStickX);
        std::size_t const iRy = static_cast<std::size_t>(AbstractControl::RStickY);
        std::size_t const iRb = static_cast<std::size_t>(AbstractControl::RShoulder);
        std::size_t const iLx = static_cast<std::size_t>(AbstractControl::LStickX);
        std::size_t const iLy = static_cast<std::size_t>(AbstractControl::LStickY);
        bool const rbHeld = controlScratch_[iRb] >= 0.5f;

        float yawIn = 0.f;
        if (w->isKeyDown(Key::Right)) {
            yawIn += 1.f;
        }
        if (w->isKeyDown(Key::Left)) {
            yawIn -= 1.f;
        }
        if (!rbHeld) {
            yawIn += controlScratch_[iRx] * 1.25f;
        }
        float zoomIn = 0.f;
        if (w->isKeyDown(Key::Up)) {
            zoomIn += 1.f;
        }
        if (w->isKeyDown(Key::Down)) {
            zoomIn -= 1.f;
        }
        if (!rbHeld) {
            zoomIn += controlScratch_[iRy] * 1.05f;
        }
        constexpr float yawSpeed = 1.75f;
        constexpr float zoomSpeed = 2.05f;
        camYawTarget += yawIn * yawSpeed * h;
        camDistTarget -= zoomIn * zoomSpeed * h;
        camDistTarget = std::clamp(camDistTarget, 0.72f, 2.4f);

        if (w->isKeyDown(Key::E)) {
            camHeightTarget += 0.95f * h;
        }
        if (w->isKeyDown(Key::C)) {
            camHeightTarget -= 0.95f * h;
        }
        camHeightTarget = std::clamp(camHeightTarget, 0.15f, 1.4f);

        camHeightTarget += (controlScratch_[iRt] - controlScratch_[iLt]) * 0.58f * h;
        camHeightTarget = std::clamp(camHeightTarget, 0.15f, 1.4f);

        bool const mouseDown = w->isMouseButtonDown(MouseButton::Left);
        bool const rightDown = w->isMouseButtonDown(MouseButton::Right);
        double mx = 0.0, my = 0.0;
        (void)w->getCursorPos(mx, my);

        constexpr float kCamTau = 11.5f;
        float const camBlend = 1.f - std::exp(-kCamTau * h);
        camYaw += (camYawTarget - camYaw) * camBlend;
        camDist += (camDistTarget - camDist) * camBlend;
        camHeight += (camHeightTarget - camHeight) * camBlend;

        float const sxRoll = std::sin(camYaw);
        float const czRoll = std::cos(camYaw);
        Vec3 const worldFwdRoll{-sxRoll, 0.f, -czRoll};
        Vec3 const worldRightRoll{-czRoll, 0.f, sxRoll};
        float ax = 0.f;
        float az = 0.f;
        if (w->isKeyDown(Key::W)) {
            az += 1.f;
        }
        if (w->isKeyDown(Key::S)) {
            az -= 1.f;
        }
        if (w->isKeyDown(Key::D)) {
            ax += 1.f;
        }
        if (w->isKeyDown(Key::A)) {
            ax -= 1.f;
        }
        ax += controlScratch_[iLx];
        az -= controlScratch_[iLy];
        float const alen = std::sqrt(ax * ax + az * az);

        bool const spaceDown = w->isKeyDown(Key::Space);
        std::size_t const iPadA2 = static_cast<std::size_t>(AbstractControl::RPadDown);
        bool const padJumpDown = controlScratch_[iPadA2] >= 0.5f;
        bool const jumpHeld = spaceDown || padJumpDown;

        if (sessionKind_ == GardenSessionKind::RemoteClient) {
            if (alen > 1e-5f) {
                Vec3 const wish = worldRightRoll * (ax / alen) + worldFwdRoll * (az / alen);
                pendingMoveX_ = wish.x;
                pendingMoveZ_ = wish.z;
            } else {
                pendingMoveX_ = 0.f;
                pendingMoveZ_ = 0.f;
            }
            pendingButtons_ = jumpHeld ? kClientInputButton_Jump : 0u;
        } else if (sessionKind_ == GardenSessionKind::ListenHost && hostListenInitialized_ && marbleCount_ > 0u) {
            if (alen > 1e-5f) {
                Vec3 const wish = worldRightRoll * (ax / alen) + worldFwdRoll * (az / alen);
                hostListenWishX_ = wish.x;
                hostListenWishZ_ = wish.z;
            } else {
                hostListenWishX_ = 0.f;
                hostListenWishZ_ = 0.f;
            }
            hostListenJumpHeld_ = jumpHeld;
        } else if (marbles[0].invMass > 0.f) {
            float wx = 0.f;
            float wz = 0.f;
            if (alen > 1e-5f) {
                Vec3 const wish = worldRightRoll * (ax / alen) + worldFwdRoll * (az / alen);
                wx = wish.x;
                wz = wish.z;
            }
            marble::garden::applyGardenMarblePlayerStep(
                marbles[0], layout, wx, wz, jumpHeld, jumpWasHeld_, jumpChargeSec_, h, kMarbleRadius);
        }

        int fbW = 1, fbH = 1;
        w->getFramebufferSize(&fbW, &fbH);
        float const aspect =
            static_cast<float>(fbW) / static_cast<float>(std::max(1, fbH));
        constexpr float fovy = 60.f * 3.14159265f / 180.f;

        std::size_t const viewMarble = localViewMarbleIndex();
        Vec3 const player = marbles[viewMarble].position;
        float const sx = std::sin(camYaw);
        float const cz = std::cos(camYaw);
        Vec3 const eye = player + Vec3{sx * camDist, camHeight, cz * camDist};
        Vec3 const target = player + Vec3{0.f, 0.06f, 0.f};

        bool const padDown = controlScratch_[iRb] >= 0.5f;

        if (sessionKind_ != GardenSessionKind::RemoteClient && sessionKind_ != GardenSessionKind::ListenHost) {

        if (mouseFlickArmed && rightDown && !rightMouseWasDown) {
            mouseFlickArmed = false;
            hasPlaneStroke = false;
        }

        if (mouseDown && !mouseWasDown) {
            Vec3 rayO{};
            Vec3 rayD{};
            worldRayFromWindowPixel(
                static_cast<float>(mx),
                static_cast<float>(my),
                fbW,
                fbH,
                eye,
                target,
                fovy,
                aspect,
                rayO,
                rayD
            );
            float tHit = 0.f;
            if (rayHitsSphere(rayO, rayD, marbles[0].position, kMarbleRadius * 1.15f, tHit)) {
                strokePlaneY = marbles[0].position.y;
                Vec3 pHit{};
                if (!rayIntersectHorizontalPlane(rayO, rayD, strokePlaneY, pHit)) {
                    pHit = Vec3{marbles[0].position.x, strokePlaneY, marbles[0].position.z};
                }
                pStrokeStart = pHit;
                pStrokeLast = pHit;
                pStrokePrev = pHit;
                hasPlaneStroke = true;
                mouseFlickArmed = true;
                strokeEmaValid = false;
            }
        }

        if (mouseFlickArmed && mouseDown) {
            Vec3 rayO{};
            Vec3 rayD{};
            worldRayFromWindowPixel(
                static_cast<float>(mx),
                static_cast<float>(my),
                fbW,
                fbH,
                eye,
                target,
                fovy,
                aspect,
                rayO,
                rayD
            );
            Vec3 p{};
            if (rayIntersectHorizontalPlane(rayO, rayD, strokePlaneY, p)) {
                Vec3 const dStep = p - pStrokeLast;
                float const planar = std::sqrt(dStep.x * dStep.x + dStep.z * dStep.z);
                if (planar > 1e-7f) {
                    float const invDt = 1.f / std::max(h, 1e-4f);
                    float const ivx = (p.x - pStrokeLast.x) * invDt;
                    float const ivz = (p.z - pStrokeLast.z) * invDt;
                    constexpr float kStrokeEma = 0.38f;
                    if (!strokeEmaValid) {
                        strokeVelEmaX = ivx;
                        strokeVelEmaZ = ivz;
                        strokeEmaValid = true;
                    } else {
                        strokeVelEmaX = strokeVelEmaX * (1.f - kStrokeEma) + ivx * kStrokeEma;
                        strokeVelEmaZ = strokeVelEmaZ * (1.f - kStrokeEma) + ivz * kStrokeEma;
                    }
                }
                pStrokePrev = pStrokeLast;
                pStrokeLast = p;
            }
        }

        if (mouseWasDown && !mouseDown && mouseFlickArmed) {
            if (hasPlaneStroke) {
                applyLawnStrokeImpulse(
                    marbles[0],
                    pStrokeStart,
                    pStrokeLast,
                    pStrokePrev,
                    strokeVelEmaX,
                    strokeVelEmaZ,
                    strokeEmaValid,
                    h
                );
            }
            mouseFlickArmed = false;
            hasPlaneStroke = false;
            strokeEmaValid = false;
        }

        constexpr float kPadStickScale = 3.55f;
        Vec3 const worldFwd{-sx, 0.f, -cz};
        Vec3 const worldRight{-cz, 0.f, sx};

        if (padDown && !padWasDown) {
            padStrokePlaneY = marbles[0].position.y;
            padPStart = Vec3{marbles[0].position.x, padStrokePlaneY, marbles[0].position.z};
            padPLast = padPStart;
            padPPrev = padPStart;
            padFlickArmed = true;
            padStrokeEmaValid = false;
        }
        if (padFlickArmed && padDown) {
            float rx = controlScratch_[iRx];
            float ry = controlScratch_[iRy];
            float const rm = std::sqrt(rx * rx + ry * ry);
            if (rm > 1e-5f) {
                rx /= rm;
                ry /= rm;
            }
            Vec3 const delta =
                worldRight * (rx * kPadStickScale * h) + worldFwd * ((-ry) * kPadStickScale * h);
            Vec3 const next = padPLast + delta;
            Vec3 const dpad = next - padPLast;
            float const planarPad = std::sqrt(dpad.x * dpad.x + dpad.z * dpad.z);
            if (planarPad > 1e-7f) {
                float const invDt = 1.f / std::max(h, 1e-4f);
                float const ivx = (next.x - padPLast.x) * invDt;
                float const ivz = (next.z - padPLast.z) * invDt;
                constexpr float kPadEma = 0.38f;
                if (!padStrokeEmaValid) {
                    padStrokeVelEmaX = ivx;
                    padStrokeVelEmaZ = ivz;
                    padStrokeEmaValid = true;
                } else {
                    padStrokeVelEmaX = padStrokeVelEmaX * (1.f - kPadEma) + ivx * kPadEma;
                    padStrokeVelEmaZ = padStrokeVelEmaZ * (1.f - kPadEma) + ivz * kPadEma;
                }
            }
            padPPrev = padPLast;
            padPLast = next;
            padPLast.y = padStrokePlaneY;
        }
        if (padWasDown && !padDown && padFlickArmed) {
            applyLawnStrokeImpulse(
                marbles[0],
                padPStart,
                padPLast,
                padPPrev,
                padStrokeVelEmaX,
                padStrokeVelEmaZ,
                padStrokeEmaValid,
                h
            );
            padFlickArmed = false;
            padStrokeEmaValid = false;
        }

        } // !RemoteClient — end of flick guard

        mouseWasDown = mouseDown;
        rightMouseWasDown = rightDown;
        padWasDown = padDown;
        }
    }

    if (!paused && sessionKind_ == GardenSessionKind::RemoteClient) {
        tickRemoteClient(h);
    } else if (!paused && sessionKind_ == GardenSessionKind::ListenHost && hostListenInitialized_ &&
               marbleCount_ > 0u) {
        hostAuthorityFixedAccum_ += h;
        static constexpr int kMaxCatchUpSteps = 4;
        float const fixedDt = kGardenAuthorityFixedDeltaSeconds;
        for (int guard = 0; guard < kMaxCatchUpSteps && hostAuthorityFixedAccum_ >= fixedDt; ++guard) {
            hostAuthorityFixedAccum_ -= fixedDt;

            ClientInputWirePayload inp{};
            inp.clientTick = hostInputTickCounter_++;
            inp.serverTickAck = hostSession_.currentTick();
            inp.steer = 0.f;
            inp.moveX = hostListenWishX_;
            inp.moveZ = hostListenWishZ_;
            inp.buttons = hostListenJumpHeld_ ? kClientInputButton_Jump : 0u;
            hostSession_.submitSyntheticClientInput(2u, inp);

            marble::garden::gardenAuthorityFixedStep(
                marble::garden::GardenAuthorityRunMode::ListenHost,
                hostSession_,
                fixedDt,
                true,
                *physicsScene_,
                layout,
                std::span<RigidBodyKinematics>(marbles.data(), marbleCount_),
                std::span<marble::physics::PhysicsBodyId const>(marbleBodyIds_.data(), marbleCount_),
                marbleCount_,
                std::span<float>(hostPeerJumpHoldSec_.data(), marbleCount_),
                std::span<bool>(hostPeerJumpWasHeld_.data(), marbleCount_),
                hostAuthorityWorld_);
        }
    } else if (!paused && sessionKind_ == GardenSessionKind::Offline) {
        float const minCenterY = layout.terrain.minHeight - kMarbleRadius - 0.55f;
        PhysicsCylindricalXZClamp const clamp{
            kGardenRadius - kMarbleRadius - 0.02f,
            minCenterY,
        };
        PhysicsStepOptions const stepOpts{&clamp, std::span(marbleBodyIds_.data(), marbleCount_)};
        physicsScene_->syncHostVelocitiesBeforeStep(
            std::span<marble::physics::PhysicsBodyId const>(marbleBodyIds_.data(), marbleCount_),
            marbles.data(),
            marbleCount_);
        physicsScene_->step(h, physics.settings(), stepOpts);
        physicsScene_->readBackKinematics(
            std::span<marble::physics::PhysicsBodyId const>(marbleBodyIds_.data(), marbleCount_),
            marbles.data(),
            marbleCount_);
    }

    if (auto* w = engine.window()) {
        char buf[256];
        if (paused) {
            if (pausePanel == PausePanel::Options) {
                if (sessionKind_ == GardenSessionKind::ListenHost) {
                    (void)std::snprintf(
                        buf,
                        sizeof(buf),
                        "Garden — Paused — Listen host — Options — Esc · O · R · Q");
                } else {
                    (void)std::snprintf(
                        buf,
                        sizeof(buf),
                        "Garden — Paused — Options — Esc · O back · R · Q menu");
                }
            } else if (sessionKind_ == GardenSessionKind::ListenHost) {
                (void)std::snprintf(
                    buf,
                    sizeof(buf),
                    "Garden — Paused — Listen host — Esc resume · O · R · Q menu");
            } else {
                (void)std::snprintf(
                    buf,
                    sizeof(buf),
                    "Garden — Paused — Esc resume · O options · R restart · Q menu");
            }
        } else if (sessionKind_ == GardenSessionKind::RemoteClient) {
            char const* connStr = "Connecting...";
            if (clientSession_.state() == ConnectionState::Connected) {
                connStr = "Connected";
            } else if (clientSession_.state() == ConnectionState::Disconnected && clientConnected_) {
                connStr = "Disconnected";
            }
            (void)std::snprintf(
                buf,
                sizeof(buf),
                "Garden - Remote [%s] - %llu snaps - Esc - arrows camera",
                connStr,
                static_cast<unsigned long long>(snapInterp_.frameCount()));
        } else if (sessionKind_ == GardenSessionKind::ListenHost) {
            (void)std::snprintf(
                buf,
                sizeof(buf),
                "Garden — Listen UDP :%u — tick %llu — Esc · WASD · Space · E/C",
                static_cast<unsigned>(hostListenPort_),
                static_cast<unsigned long long>(ctx.frameIndex));
        } else {
            (void)std::snprintf(
                buf,
                sizeof(buf),
                "Garden — Esc · WASD roll · arrows camera · Space jump · E/C height · click flick");
        }
        w->setTitle(buf);
    }
}
} // namespace marble::garden_app
