#include "input/PlatformKeyboardBridge.hpp"

#include "platform/window/Window.hpp"

#include <algorithm>

namespace marble::input {

void sampleKeyboardIntoAbstractControls(platform::Window const& window, AbstractControlArray& out) noexcept {
    out.fill(0.f);

    using marble::platform::Key;

    if (window.isKeyDown(Key::W) || window.isKeyDown(Key::Up)) {
        out[static_cast<std::size_t>(AbstractControl::LPadUp)] = 1.f;
    }
    if (window.isKeyDown(Key::S) || window.isKeyDown(Key::Down)) {
        out[static_cast<std::size_t>(AbstractControl::LPadDown)] = 1.f;
    }
    if (window.isKeyDown(Key::A) || window.isKeyDown(Key::Left)) {
        out[static_cast<std::size_t>(AbstractControl::LPadLeft)] = 1.f;
    }
    if (window.isKeyDown(Key::D) || window.isKeyDown(Key::Right)) {
        out[static_cast<std::size_t>(AbstractControl::LPadRight)] = 1.f;
    }
    if (window.isKeyDown(Key::Escape)) {
        out[static_cast<std::size_t>(AbstractControl::BackSelect)] = 1.f;
    }
    if (window.isKeyDown(Key::Enter) || window.isKeyDown(Key::Space)) {
        out[static_cast<std::size_t>(AbstractControl::Start)] = 1.f;
    }
    if (window.isKeyDown(Key::Tab)) {
        out[static_cast<std::size_t>(AbstractControl::RStickButton)] = 1.f;
    }
    if (window.isKeyDown(Key::Q)) {
        out[static_cast<std::size_t>(AbstractControl::LShoulder)] = 1.f;
    }
    if (window.isKeyDown(Key::E)) {
        out[static_cast<std::size_t>(AbstractControl::RShoulder)] = 1.f;
    }
    if (window.isKeyDown(Key::F)) {
        out[static_cast<std::size_t>(AbstractControl::LStickButton)] = 1.f;
    }
    if (window.isKeyDown(Key::LeftShift)) {
        out[static_cast<std::size_t>(AbstractControl::RStickButton)] =
            std::max(out[static_cast<std::size_t>(AbstractControl::RStickButton)], 1.f);
    }
    if (window.isKeyDown(Key::LeftControl)) {
        out[static_cast<std::size_t>(AbstractControl::LStickButton)] =
            std::max(out[static_cast<std::size_t>(AbstractControl::LStickButton)], 1.f);
    }
    if (window.isKeyDown(Key::Digit1)) {
        out[static_cast<std::size_t>(AbstractControl::RPadUp)] = 1.f;
    }
    if (window.isKeyDown(Key::Digit2)) {
        out[static_cast<std::size_t>(AbstractControl::RPadDown)] = 1.f;
    }
}

} // namespace marble::input
