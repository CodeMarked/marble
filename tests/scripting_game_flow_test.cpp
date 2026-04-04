#include "gameplay/ScriptingAndGameFlow.hpp"

#include "core/StringId.hpp"

namespace {

void bump(void* userData, marble::gameplay::ScriptArg arg) noexcept {
    auto* counter = static_cast<int*>(userData);
    *counter += static_cast<int>(arg);
}

} // namespace

int main() {
    using marble::core::StringId;
    using namespace marble::core::literals;
    using namespace marble::gameplay;

    int counter = 0;
    ScriptHost<4> host{};
    const StringId idA = "on_tick"_sid;
    const StringId idB = "add"_sid;

    if (!host.registerBinding(idA, &bump, &counter)) {
        return 1;
    }
    if (!host.registerBinding(idB, &bump, &counter)) {
        return 2;
    }
    if (host.registerBinding(idA, &bump, &counter)) {
        return 3;
    }
    if (host.invoke("missing"_sid, 1)) {
        return 4;
    }
    if (!host.invoke(idA, 10)) {
        return 5;
    }
    if (counter != 10) {
        return 6;
    }
    if (!host.invoke(idB, -3)) {
        return 7;
    }
    if (counter != 7) {
        return 8;
    }
    if (host.registerBinding(StringId{}, &bump, &counter)) {
        return 9;
    }
    if (host.registerBinding(idB, nullptr, &counter)) {
        return 10;
    }

    ScriptHost<2> small{};
    if (!small.registerBinding("x"_sid, &bump, &counter)) {
        return 11;
    }
    if (!small.registerBinding("y"_sid, &bump, &counter)) {
        return 12;
    }
    if (small.registerBinding("z"_sid, &bump, &counter)) {
        return 13;
    }

    GameFlowController flow{};
    if (flow.phase() != GameFlowPhase::Boot) {
        return 20;
    }
    if (flow.tryTransition(GameFlowPhase::Playing)) {
        return 21;
    }
    if (!flow.tryTransition(GameFlowPhase::Loading)) {
        return 22;
    }
    if (!flow.tryTransition(GameFlowPhase::Playing)) {
        return 23;
    }
    if (!flow.tryTransition(GameFlowPhase::Paused)) {
        return 24;
    }
    if (flow.tryTransition(GameFlowPhase::Loading)) {
        return 25;
    }
    if (!flow.tryTransition(GameFlowPhase::Playing)) {
        return 26;
    }
    if (!flow.tryTransition(GameFlowPhase::Exiting)) {
        return 27;
    }
    if (flow.tryTransition(GameFlowPhase::Playing)) {
        return 28;
    }

    flow.reset();
    if (flow.phase() != GameFlowPhase::Boot) {
        return 29;
    }

    if (!gameFlowTransitionAllowed(GameFlowPhase::Boot, GameFlowPhase::Loading)) {
        return 30;
    }
    if (gameFlowTransitionAllowed(GameFlowPhase::Boot, GameFlowPhase::Boot)) {
        return 31;
    }
    if (gameFlowTransitionAllowed(GameFlowPhase::Exiting, GameFlowPhase::Boot)) {
        return 32;
    }

    return 0;
}
