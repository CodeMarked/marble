#pragma once

#include <memory>
#include <string>

#include <vulkan/vulkan.h>

namespace marble::platform {

/// Cross-platform window abstraction.
/// Isolates GLFW from the rest of the engine; no GLFW types appear in this header.
class Window {
public:
    /// Creates a window. Returns null if creation fails.
    static std::unique_ptr<Window> create(const std::string& title, int width, int height);

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    /// True when the user has requested close (e.g. close button).
    bool shouldClose() const;

    /// Process pending window/input events. Call once per frame.
    void pollEvents();

    /// Window client area dimensions (logical size).
    int width() const;
    int height() const;

    /// Framebuffer size (may differ from width/height on HiDPI). Use for Vulkan swapchain.
    void getFramebufferSize(int* outWidth, int* outHeight) const;

    /// Create a VkSurfaceKHR for this window. Caller must destroy the surface with vkDestroySurfaceKHR.
    /// Returns true on success.
    bool createVulkanSurface(VkInstance instance, VkSurfaceKHR* outSurface) const;

private:
    struct Impl;
    explicit Window(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace marble::platform
