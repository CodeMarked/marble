#include "platform/window/Window.hpp"
#include <iostream>

int main()
{
    std::cout << "Marble game starting\n";

    auto window = marble::platform::Window::create("Marble", 1280, 720);
    if (!window) {
        std::cerr << "Failed to create window\n";
        return 1;
    }

    while (!window->shouldClose()) {
        window->pollEvents();
    }

    std::cout << "Window closed.\n";
    return 0;
}
