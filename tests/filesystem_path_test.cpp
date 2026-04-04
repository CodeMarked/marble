#include "platform/filesystem/Path.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

int main() {
    namespace fs = marble::platform::filesystem;

    const std::filesystem::path base{"assets"};
    const std::filesystem::path leaf{"textures/../textures/diffuse.png"};
    const std::filesystem::path joined = fs::join(base, leaf);
    if (fs::filenameOf(joined) != "diffuse.png") {
        return 1;
    }
    if (fs::extensionOf(joined) != ".png") {
        return 2;
    }

    const std::filesystem::path dir = fs::directoryOf(joined);
    if (dir.filename().string() != "textures") {
        return 3;
    }

#if defined(_WIN32)
    const std::vector<std::filesystem::path> parts = fs::splitSearchPath("a;b;c");
#else
    const std::vector<std::filesystem::path> parts = fs::splitSearchPath("a:b:c");
#endif
    if (parts.size() != 3 || parts[0].string() != "a" || parts[2].string() != "c") {
        return 4;
    }

    // Relative path should not report absolute.
    if (fs::isAbsolute(std::filesystem::path{"foo/bar"})) {
        return 5;
    }

    return 0;
}
