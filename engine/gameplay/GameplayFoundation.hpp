#pragma once

#include "math/Vec3.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace marble::gameplay {

using ObjectId = std::uint32_t;

struct GameplayObject {
    ObjectId id{};
    std::uint32_t archetypeId{};
    math::Vec3 position{};
    bool enabled{true};
    bool occupied{};
};

enum class EditorCommandType : std::uint8_t {
    AddObject,
    RemoveObject,
    MoveObject,
    ToggleEnabled
};

struct EditorCommand {
    EditorCommandType type{};
    ObjectId id{};
    std::uint32_t archetypeId{};
    math::Vec3 position{};
    bool enabled{true};
};

template <std::size_t MaxObjects, std::size_t MaxCommands>
class GameplayWorld {
public:
    static_assert(MaxObjects > 0, "GameplayWorld requires object capacity");
    static_assert(MaxCommands > 0, "GameplayWorld requires command capacity");

    [[nodiscard]] bool spawn(ObjectId id, std::uint32_t archetypeId, math::Vec3 position) noexcept {
        GameplayObject* existing = findMutable(id);
        if (existing != nullptr) {
            existing->archetypeId = archetypeId;
            existing->position = position;
            existing->enabled = true;
            existing->occupied = true;
            return true;
        }
        GameplayObject* slot = firstFreeSlot();
        if (slot == nullptr) {
            return false;
        }
        *slot = GameplayObject{id, archetypeId, position, true, true};
        return true;
    }

    [[nodiscard]] bool destroy(ObjectId id) noexcept {
        GameplayObject* object = findMutable(id);
        if (object == nullptr) {
            return false;
        }
        *object = {};
        return true;
    }

    [[nodiscard]] GameplayObject* findMutable(ObjectId id) noexcept {
        for (GameplayObject& object : objects_) {
            if (object.occupied && object.id == id) {
                return &object;
            }
        }
        return nullptr;
    }

    [[nodiscard]] GameplayObject const* find(ObjectId id) const noexcept {
        return const_cast<GameplayWorld*>(this)->findMutable(id);
    }

    [[nodiscard]] std::size_t activeCount() const noexcept {
        std::size_t n = 0;
        for (GameplayObject const& object : objects_) {
            if (object.occupied) {
                ++n;
            }
        }
        return n;
    }

    template <typename Fn>
    void forEachActive(Fn&& fn) {
        for (GameplayObject& object : objects_) {
            if (object.occupied && object.enabled) {
                fn(object);
            }
        }
    }

    [[nodiscard]] bool queueEditorCommand(EditorCommand const& command) noexcept {
        if (queuedCount_ >= MaxCommands) {
            return false;
        }
        queued_[queuedCount_++] = command;
        return true;
    }

    void applyQueuedEditorCommands() noexcept {
        for (std::size_t i = 0; i < queuedCount_; ++i) {
            applyEditorCommand(queued_[i]);
        }
        queuedCount_ = 0;
    }

    [[nodiscard]] std::size_t queuedEditorCommandCount() const noexcept {
        return queuedCount_;
    }

private:
    void applyEditorCommand(EditorCommand const& command) noexcept {
        switch (command.type) {
        case EditorCommandType::AddObject:
            (void)spawn(command.id, command.archetypeId, command.position);
            break;
        case EditorCommandType::RemoveObject:
            (void)destroy(command.id);
            break;
        case EditorCommandType::MoveObject: {
            GameplayObject* object = findMutable(command.id);
            if (object != nullptr) {
                object->position = command.position;
            }
            break;
        }
        case EditorCommandType::ToggleEnabled: {
            GameplayObject* object = findMutable(command.id);
            if (object != nullptr) {
                object->enabled = command.enabled;
            }
            break;
        }
        }
    }

    [[nodiscard]] GameplayObject* firstFreeSlot() noexcept {
        for (GameplayObject& object : objects_) {
            if (!object.occupied) {
                return &object;
            }
        }
        return nullptr;
    }

    std::array<GameplayObject, MaxObjects> objects_{};
    std::array<EditorCommand, MaxCommands> queued_{};
    std::size_t queuedCount_{};
};

} // namespace marble::gameplay
