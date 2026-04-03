#include "platform/paths/ExecutableDir.hpp"

#include <filesystem>

int main() {
    std::filesystem::path const p = marble::platform::executableDirectory();
    if (p.empty()) {
        return 1;
    }
    if (!p.is_absolute()) {
        return 2;
    }
    return 0;
}
