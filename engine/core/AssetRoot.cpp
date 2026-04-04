#include "core/AssetRoot.hpp"
#include "core/Log.hpp"

#include "platform/paths/ExecutableDir.hpp"

#include <cstdlib>
#include <optional>
#include <string>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

namespace marble::core {

namespace {

constexpr LogChannelMask kEngineLog = static_cast<LogChannelMask>(LogChannel::Engine);

#if defined(_MSC_VER)
std::optional<std::string> readEnvVar(const char* name) {
    char* buffer = nullptr;
    size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    return value;
}
#else
std::optional<std::string> readEnvVar(const char* name) {
    if (const char* value = std::getenv(name)) {
        return std::string(value);
    }
    return std::nullopt;
}
#endif

bool tryCandidate(const std::filesystem::path& candidate, std::filesystem::path& out) {
    std::error_code ec;
    if (std::filesystem::is_directory(candidate, ec)) {
        out = candidate;
        return true;
    }
    return false;
}

} // namespace

std::optional<std::filesystem::path> resolveAssetsRoot(
    bool enabled,
    std::string_view overridePath,
    bool requireExistingDirectory
) {
    if (!enabled) {
        return std::nullopt;
    }

    std::filesystem::path chosen;

    const auto envRootOpt = readEnvVar("MARBLE_ASSETS_ROOT");
    if (envRootOpt.has_value()) {
        const std::string& envRoot = *envRootOpt;
        if (!envRoot.empty() && tryCandidate(std::filesystem::path(envRoot), chosen)) {
            return chosen;
        }
        if (requireExistingDirectory) {
            (void)logPrintf(0, kEngineLog, "Asset root resolution failed: MARBLE_ASSETS_ROOT is not a directory");
            return std::nullopt;
        }
    }

    if (!overridePath.empty()) {
        std::filesystem::path overridePathFs(overridePath);
        if (overridePathFs.is_relative()) {
            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec) {
                overridePathFs = cwd / overridePathFs;
            }
        }
        if (tryCandidate(overridePathFs, chosen)) {
            return chosen;
        }
        if (requireExistingDirectory) {
            (void)logPrintf(0, kEngineLog, "Asset root resolution failed: override path is not a directory");
            return std::nullopt;
        }
    }

    const auto exeDir = marble::platform::executableDirectory();
    if (!exeDir.empty()) {
        const auto nextToExe = exeDir / "assets";
        if (tryCandidate(nextToExe, chosen)) {
            return chosen;
        }
    }

    if (requireExistingDirectory) {
        (void)logPrintf(0, kEngineLog, "Asset root resolution failed: no valid assets directory found");
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace marble::core
