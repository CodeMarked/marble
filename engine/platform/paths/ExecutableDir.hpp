#pragma once

#include <filesystem>

namespace marble::platform {

/// Directory containing the running executable, or empty if unknown.
std::filesystem::path executableDirectory();

} // namespace marble::platform
