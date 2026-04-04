#include "core/Assert.hpp"

#include <cassert>

namespace {

int tagForTwoCases(int x) {
    switch (x) {
    case 0:
        return 10;
    case 1:
        return 20;
    }
    MARBLE_UNREACHABLE();
}

} // namespace

int main() {
    assert(tagForTwoCases(0) == 10);
    assert(tagForTwoCases(1) == 20);

    MARBLE_ASSERT(true);
    return 0;
}
