#pragma once

#include <memory>
#include <string>

#include <vulkan/vulkan.h>

namespace marble::platform {

/// Virtual key codes for `isKeyDown` (mapped from GLFW in the implementation).
enum class Key : int {
    W,
    A,
    S,
    D,
    Left,
    Right,
    Up,
    Down,
    Space,
    Escape,
};

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

    /// True once after the framebuffer size changes (e.g. window resize). Cleared when read.
    bool consumeFramebufferResized();

    /// Whether `key` is currently held (edge polling; call after `pollEvents` each frame).
    bool isKeyDown(Key key) const;

    /// Update the window title (e.g. score / timer HUD).
    void setTitle(const std::string& title);

    /// Request close on the next frame (`shouldClose()` becomes true).
    void requestClose();

    /// Used by the platform framebuffer callback (sets `consumeFramebufferResized`).
    void markFramebufferResized();

    /// Create a VkSurfaceKHR for this window. Caller must destroy the surface with vkDestroySurfaceKHR.
    /// Returns true on success.
    bool createVulkanSurface(VkInstance instance, VkSurfaceKHR* outSurface) const;

private:
    struct Impl;
    explicit Window(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace marble::platform
