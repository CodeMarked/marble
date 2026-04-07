#include "MarbleApp.hpp"

#include "core/Engine.hpp"
#include "core/Log.hpp"
#include "platform/paths/ExecutableDir.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>

int main(int argc, char** argv)
{
    using marble::core::LogChannel;
    using marble::core::LogChannelMask;
    using marble::core::logPrintf;

    constexpr LogChannelMask kGeneral = static_cast<LogChannelMask>(LogChannel::General);

    (void)logPrintf(0, kGeneral, "Marble: unified launcher (windowed = main menu; --headless skips menu)");

    marble::core::Engine::Config config {};
    config.appName = "Marble - launcher";
    config.resolveAssetsRoot = true;
    std::optional<std::uint32_t> physicalDeviceIndex;
    std::string joinHost;
    std::uint16_t joinPort = 27778u;
    bool joinMode = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--headless") {
            config.headless = true;
            continue;
        }
        if (arg == "--join" && (i + 1) < argc) {
            joinHost = argv[++i];
            joinMode = true;
            continue;
        }
        if (arg == "--join-port" && (i + 1) < argc) {
            try {
                unsigned long const v = std::stoul(argv[++i]);
                if (v > 65535ul) {
                    (void)logPrintf(0, kGeneral, "Invalid value for --join-port");
                    return 2;
                }
                joinPort = static_cast<std::uint16_t>(v);
            } catch (...) {
                (void)logPrintf(0, kGeneral, "Invalid value for --join-port");
                return 2;
            }
            continue;
        }
        if (arg == "--assets" && (i + 1) < argc) {
            config.assetsRootOverride = argv[++i];
            continue;
        }
        if (arg == "--require-assets") {
            config.requireAssetsDirectory = true;
            continue;
        }
        if (arg == "--frames" && (i + 1) < argc) {
            try {
                config.maxFrames = static_cast<unsigned long long>(std::stoull(argv[++i]));
            } catch (...) {
                (void)logPrintf(0, kGeneral, "Invalid value for --frames");
                return 2;
            }
            continue;
        }
        if (arg == "--diag-interval" && (i + 1) < argc) {
            try {
                config.diagnosticsIntervalSeconds = std::stod(argv[++i]);
            } catch (...) {
                (void)logPrintf(0, kGeneral, "Invalid value for --diag-interval");
                return 2;
            }
            continue;
        }
        if (arg == "--gpu" && (i + 1) < argc) {
            try {
                unsigned long const v = std::stoul(argv[++i]);
                if (v > static_cast<unsigned long>(std::numeric_limits<std::uint32_t>::max())) {
                    (void)logPrintf(0, kGeneral, "Invalid value for --gpu");
                    return 2;
                }
                physicalDeviceIndex = static_cast<std::uint32_t>(v);
            } catch (...) {
                (void)logPrintf(0, kGeneral, "Invalid value for --gpu");
                return 2;
            }
            continue;
        }
    }

    marble::core::Engine engine(config);

    if (!engine.init()) {
        (void)logPrintf(0, kGeneral, "Failed to initialize engine");
        return 1;
    }

    int exitCode = 0;
    {
        std::filesystem::path const exeDir = marble::platform::executableDirectory();
        std::string const shaderDir = (exeDir / "shaders").string();

        if (joinMode) {
            exitCode = marble::marbles_app::runGameplaySession(
                engine,
                marble::marbles_app::PostLandingAction::GardenRemoteClient,
                shaderDir,
                physicalDeviceIndex
            );
        } else if (config.headless) {
            exitCode = marble::marbles_app::runGameplaySession(
                engine,
                marble::marbles_app::PostLandingAction::Marbles,
                shaderDir,
                physicalDeviceIndex
            );
        } else {
            exitCode = marble::marbles_app::runWindowedGameLoop(engine, shaderDir, physicalDeviceIndex);
        }
    }

    engine.shutdown();

    (void)logPrintf(0, kGeneral, "Engine stopped.");
    return exitCode;
}
