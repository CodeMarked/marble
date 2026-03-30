#include "core/ClosedHashTable.hpp"

#include <cstddef>
#include <cstdlib>
#include <string>

namespace {

struct SmallHash {
    std::size_t operator()(int k) const noexcept { return static_cast<std::size_t>(k % 5); }
};

} // namespace

int main() {
    marble::core::ClosedHashTable<int, std::string, 5, SmallHash> table;
    if (!table.empty() || table.size() != 0 || table.capacity() != 5) {
        return 1;
    }

    if (!table.insertOrAssign(1, "one")) {
        return 2;
    }
    if (!table.insertOrAssign(6, "six")) { // collision with key 1, probes next slot
        return 3;
    }
    if (!table.insertOrAssign(11, "eleven")) { // another collision chain
        return 4;
    }
    if (table.size() != 3) {
        return 5;
    }

    const std::string* s1 = table.find(1);
    const std::string* s6 = table.find(6);
    const std::string* s11 = table.find(11);
    if (s1 == nullptr || *s1 != "one" || s6 == nullptr || *s6 != "six" || s11 == nullptr || *s11 != "eleven") {
        return 6;
    }

    if (!table.insertOrAssign(6, "SIX")) {
        return 7;
    }
    s6 = table.find(6);
    if (s6 == nullptr || *s6 != "SIX") {
        return 8;
    }

    if (!table.erase(6)) {
        return 9;
    }
    if (table.find(6) != nullptr) {
        return 10;
    }

    // Reuse a deleted slot via probing.
    if (!table.insertOrAssign(16, "sixteen")) {
        return 11;
    }
    const std::string* s16 = table.find(16);
    if (s16 == nullptr || *s16 != "sixteen") {
        return 12;
    }

    table.clear();
    if (!table.empty() || table.size() != 0) {
        return 13;
    }
    if (table.find(1) != nullptr) {
        return 14;
    }

    return 0;
}
