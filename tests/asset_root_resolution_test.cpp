#include "core/AssetRoot.hpp"

#include <filesystem>

int main() {
    using marble::core::resolveAssetsRoot;

    if (resolveAssetsRoot(false, "/should/not/matter", true).has_value()) {
        return 1;
    }

    const auto tempRoot = std::filesystem::temp_directory_path() / "marble_asset_root_test";
    std::filesystem::create_directories(tempRoot);

    const auto resolved = resolveAssetsRoot(true, tempRoot.string(), true);
    if (!resolved.has_value() || *resolved != tempRoot) {
        return 2;
    }

    const auto missingPath = std::filesystem::temp_directory_path() / "marble_nonexistent_assets_424242";
    const auto missing = resolveAssetsRoot(true, missingPath.string(), true);
    if (missing.has_value()) {
        return 3;
    }

    return 0;
}
