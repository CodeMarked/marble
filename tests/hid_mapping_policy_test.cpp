#include "input/HidMapping.hpp"

int main() {
    marble::input::InputRemapTable table;

    // Valid: digital to digital.
    if (!table.bind(
            marble::input::AbstractControl::RPadDown,
            marble::input::ActionBinding{1u, marble::input::ControlValueClass::DigitalButton, false})) {
        return 1;
    }

    // Invalid: bidirectional axis control bound as digital.
    if (table.bind(
            marble::input::AbstractControl::LStickX,
            marble::input::ActionBinding{2u, marble::input::ControlValueClass::DigitalButton, false})) {
        return 2;
    }

    // Valid: trigger as unidirectional axis.
    if (!table.bind(
            marble::input::AbstractControl::LTrigger,
            marble::input::ActionBinding{3u, marble::input::ControlValueClass::UniAxis, false})) {
        return 3;
    }

    auto b = table.bindingOf(marble::input::AbstractControl::LTrigger);
    if (!b.has_value() || b->action != 3u) {
        return 4;
    }
    table.unbind(marble::input::AbstractControl::LTrigger);
    if (table.bindingOf(marble::input::AbstractControl::LTrigger).has_value()) {
        return 5;
    }

    marble::input::ControllerRouter<4, 4> router;
    router.setAttached(0, true);
    router.setAttached(1, true);
    router.assignOneToOne();
    if (router.playerForController(0) != 0 || router.playerForController(1) != 1) {
        return 6;
    }

    // Reassign player 1 to controller 0; player 1 should no longer map to controller 1.
    if (!router.assign(0, 1)) {
        return 7;
    }
    if (router.playerForController(0) != 1) {
        return 8;
    }
    if (router.controllerForPlayer(1) != 0) {
        return 9;
    }

    // Detaching unassigns.
    router.setAttached(0, false);
    if (router.playerForController(0) != -1) {
        return 10;
    }

    return 0;
}
