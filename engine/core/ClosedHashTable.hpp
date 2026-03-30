#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace marble::core {

/// Fixed-capacity closed hash table with linear probing (book §6.3.6.1/.3).
/// Stores key/value pairs directly in slots; no dynamic allocation on insert.
template <typename K, typename V, std::size_t Capacity, typename Hash = std::hash<K>>
class ClosedHashTable {
public:
    static_assert(Capacity > 0, "ClosedHashTable capacity must be > 0");

    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    void clear() noexcept {
        for (Slot& s : slots_) {
            s.state = SlotState::Empty;
        }
        size_ = 0;
    }

    [[nodiscard]] bool insertOrAssign(K key, V value) noexcept {
        std::size_t firstDeleted = Capacity;
        const std::size_t start = bucketIndex(key);

        for (std::size_t step = 0; step < Capacity; ++step) {
            const std::size_t idx = (start + step) % Capacity;
            Slot& slot = slots_[idx];

            if (slot.state == SlotState::Occupied) {
                if (slot.key == key) {
                    slot.value = std::move(value);
                    return true;
                }
                continue;
            }

            if (slot.state == SlotState::Deleted && firstDeleted == Capacity) {
                firstDeleted = idx;
                continue;
            }

            const std::size_t target = (firstDeleted != Capacity) ? firstDeleted : idx;
            slots_[target].key = std::move(key);
            slots_[target].value = std::move(value);
            slots_[target].state = SlotState::Occupied;
            ++size_;
            return true;
        }

        if (firstDeleted != Capacity) {
            Slot& slot = slots_[firstDeleted];
            slot.key = std::move(key);
            slot.value = std::move(value);
            slot.state = SlotState::Occupied;
            ++size_;
            return true;
        }

        return false;
    }

    [[nodiscard]] V* find(K const& key) noexcept {
        const std::size_t start = bucketIndex(key);
        for (std::size_t step = 0; step < Capacity; ++step) {
            const std::size_t idx = (start + step) % Capacity;
            Slot& slot = slots_[idx];
            if (slot.state == SlotState::Empty) {
                return nullptr;
            }
            if (slot.state == SlotState::Occupied && slot.key == key) {
                return &slot.value;
            }
        }
        return nullptr;
    }

    [[nodiscard]] V const* find(K const& key) const noexcept {
        return const_cast<ClosedHashTable*>(this)->find(key);
    }

    [[nodiscard]] bool erase(K const& key) noexcept {
        const std::size_t start = bucketIndex(key);
        for (std::size_t step = 0; step < Capacity; ++step) {
            const std::size_t idx = (start + step) % Capacity;
            Slot& slot = slots_[idx];
            if (slot.state == SlotState::Empty) {
                return false;
            }
            if (slot.state == SlotState::Occupied && slot.key == key) {
                slot.state = SlotState::Deleted;
                --size_;
                return true;
            }
        }
        return false;
    }

    template <typename Fn>
    void forEachOccupied(Fn&& fn) {
        for (Slot& slot : slots_) {
            if (slot.state == SlotState::Occupied) {
                fn(slot.key, slot.value);
            }
        }
    }

    template <typename Fn>
    void forEachOccupied(Fn&& fn) const {
        for (Slot const& slot : slots_) {
            if (slot.state == SlotState::Occupied) {
                fn(slot.key, slot.value);
            }
        }
    }

private:
    enum class SlotState : unsigned char { Empty, Occupied, Deleted };

    struct Slot {
        K key{};
        V value{};
        SlotState state{SlotState::Empty};
    };

    [[nodiscard]] std::size_t bucketIndex(K const& key) const noexcept {
        return Hash{}(key) % Capacity;
    }

    std::array<Slot, Capacity> slots_{};
    std::size_t size_{0};
};

} // namespace marble::core
