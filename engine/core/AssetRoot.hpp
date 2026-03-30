#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace marble::core {

/// Resolves the runtime directory that holds shipped game assets (runtime stage of the asset pipeline).
///
/// Resolution order when enabled:
/// 1. `MARBLE_ASSETS_ROOT` environment variable (non-empty)
/// 2. `overridePath` when non-empty (relative paths resolve against current working directory)
/// 3. `<executable_directory>/assets` when executable directory is known
///
/// Returns the first path that exists and is a directory. When `requireExistingDirectory` is true
/// and no candidate qualifies, returns `std::nullopt`.
std::optional<std::filesystem::path> resolveAssetsRoot(
    bool enabled,
    std::string_view overridePath,
    bool requireExistingDirectory
);

} // namespace marble::core
