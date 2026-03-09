#include "platform/window/Window.hpp"

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>

namespace marble::platform {

struct Window::Impl {
    GLFWwindow* window = nullptr;
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
    return std::unique_ptr<Window>(new Window(std::move(impl)));
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
