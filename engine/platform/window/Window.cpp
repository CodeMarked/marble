#include "platform/window/Window.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>

namespace marble::platform {

namespace {

void framebufferResizeThunk(GLFWwindow* gw, int /*width*/, int /*height*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(gw));
    if (self) {
        self->markFramebufferResized();
    }
}

} // namespace

struct Window::Impl {
    GLFWwindow* window = nullptr;
    bool framebufferResized = false;
};

namespace {

void onError(int code, const char* message) {
    (void)code;
    // Could route to engine logging when available.
    fprintf(stderr, "GLFW error: %s\n", message);
}

} // namespace

std::unique_ptr<Window> Window::create(const std::string& title, int width, int height) {
    static bool glfwInitialized = false;
    if (!glfwInitialized) {
        glfwSetErrorCallback(onError);
        // GLFW dlopen("libvulkan.1.dylib") often fails when the app already linked the loader
        // via a full path (e.g. Homebrew). Reuse the same entry point as our Vulkan link.
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
        glfwInitVulkanLoader(vkGetInstanceProcAddr);
#endif
        if (glfwInit() != GLFW_TRUE) {
            return nullptr;
        }
        glfwInitialized = true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* raw = glfwCreateWindow(
        width,
        height,
        title.c_str(),
        nullptr,
        nullptr
    );
    if (!raw) {
        return nullptr;
    }

    auto impl = std::make_unique<Impl>();
    impl->window = raw;
    auto* win = new Window(std::move(impl));
    glfwSetWindowUserPointer(raw, win);
    glfwSetFramebufferSizeCallback(raw, framebufferResizeThunk);
    return std::unique_ptr<Window>(win);
}

Window::Window(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Window::~Window() {
    if (impl_ && impl_->window) {
        glfwDestroyWindow(impl_->window);
        impl_->window = nullptr;
    }
}

bool Window::shouldClose() const {
    return impl_ && impl_->window && glfwWindowShouldClose(impl_->window) != 0;
}

void Window::pollEvents() {
    glfwPollEvents();
}

int Window::width() const {
    if (!impl_ || !impl_->window) return 0;
    int w = 0, h = 0;
    glfwGetWindowSize(impl_->window, &w, &h);
    return w;
}

int Window::height() const {
    if (!impl_ || !impl_->window) return 0;
    int w = 0, h = 0;
    glfwGetWindowSize(impl_->window, &w, &h);
    return h;
}

void Window::getFramebufferSize(int* outWidth, int* outHeight) const {
    if (!impl_ || !impl_->window || !outWidth || !outHeight) {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        return;
    }
    glfwGetFramebufferSize(impl_->window, outWidth, outHeight);
}

bool Window::consumeFramebufferResized() {
    if (!impl_) {
        return false;
    }
    const bool v = impl_->framebufferResized;
    impl_->framebufferResized = false;
    return v;
}

bool Window::isKeyDown(Key key) const {
    if (!impl_ || !impl_->window) {
        return false;
    }
    int glfwKey = GLFW_KEY_UNKNOWN;
    switch (key) {
    case Key::W:
        glfwKey = GLFW_KEY_W;
        break;
    case Key::A:
        glfwKey = GLFW_KEY_A;
        break;
    case Key::S:
        glfwKey = GLFW_KEY_S;
        break;
    case Key::D:
        glfwKey = GLFW_KEY_D;
        break;
    case Key::Left:
        glfwKey = GLFW_KEY_LEFT;
        break;
    case Key::Right:
        glfwKey = GLFW_KEY_RIGHT;
        break;
    case Key::Up:
        glfwKey = GLFW_KEY_UP;
        break;
    case Key::Down:
        glfwKey = GLFW_KEY_DOWN;
        break;
    case Key::Space:
        glfwKey = GLFW_KEY_SPACE;
        break;
    case Key::Escape:
        glfwKey = GLFW_KEY_ESCAPE;
        break;
    case Key::Enter:
        return glfwGetKey(impl_->window, GLFW_KEY_ENTER) == GLFW_PRESS ||
            glfwGetKey(impl_->window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    case Key::Tab:
        glfwKey = GLFW_KEY_TAB;
        break;
    case Key::LeftShift:
        glfwKey = GLFW_KEY_LEFT_SHIFT;
        break;
    case Key::LeftControl:
        glfwKey = GLFW_KEY_LEFT_CONTROL;
        break;
    case Key::Q:
        glfwKey = GLFW_KEY_Q;
        break;
    case Key::E:
        glfwKey = GLFW_KEY_E;
        break;
    case Key::C:
        glfwKey = GLFW_KEY_C;
        break;
    case Key::R:
        glfwKey = GLFW_KEY_R;
        break;
    case Key::T:
        glfwKey = GLFW_KEY_T;
        break;
    case Key::O:
        glfwKey = GLFW_KEY_O;
        break;
    case Key::F:
        glfwKey = GLFW_KEY_F;
        break;
    case Key::Digit1:
        glfwKey = GLFW_KEY_1;
        break;
    case Key::Digit2:
        glfwKey = GLFW_KEY_2;
        break;
    }
    return glfwGetKey(impl_->window, glfwKey) == GLFW_PRESS;
}

bool Window::getCursorPos(double& outX, double& outY) const {
    if (!impl_ || !impl_->window) {
        return false;
    }
    glfwGetCursorPos(impl_->window, &outX, &outY);
    return true;
}

bool Window::isMouseButtonDown(MouseButton button) const {
    if (!impl_ || !impl_->window) {
        return false;
    }
    int const b = static_cast<int>(button);
    return glfwGetMouseButton(impl_->window, b) == GLFW_PRESS;
}

void Window::setTitle(const std::string& title) {
    if (impl_ && impl_->window) {
        glfwSetWindowTitle(impl_->window, title.c_str());
    }
}

void Window::requestClose() {
    if (impl_ && impl_->window) {
        glfwSetWindowShouldClose(impl_->window, GLFW_TRUE);
    }
}

void Window::markFramebufferResized() {
    if (impl_) {
        impl_->framebufferResized = true;
    }
}

bool Window::createVulkanSurface(VkInstance instance, VkSurfaceKHR* outSurface) const {
    if (!impl_ || !impl_->window || !instance || !outSurface) {
        return false;
    }
    VkResult err = glfwCreateWindowSurface(
        instance,
        impl_->window,
        nullptr,
        outSurface
    );
    return err == VK_SUCCESS;
}

} // namespace marble::platform
