#include "core/StringId.hpp"

#include <cstdlib>

using namespace marble::core::literals;

static_assert("player"_sid == marble::core::makeStringId("player"));
static_assert("player"_sid != "enemy"_sid);

int main() {
    const auto a = marble::core::makeStringId("anim-walk");
    const auto b = marble::core::makeStringId("anim-walk");
    const auto c = marble::core::makeStringId("anim-jump");
    if (!(a == b) || (a == c)) {
        return 1;
    }
    return 0;
}
