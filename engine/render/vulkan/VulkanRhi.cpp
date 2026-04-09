#include "render/vulkan/VulkanRhi.hpp"

#include "render/DrawFlags.hpp"
#include "math/Mat4.hpp"
#include "platform/window/Window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

#if defined(MARBLE_DEBUG)
#define MARBLE_VK_ENABLE_VALIDATION 1
#else
#define MARBLE_VK_ENABLE_VALIDATION 0
#endif

namespace marble::render {

namespace {

// SPIR-V modules are small; cap reads to avoid trivial memory DoS from huge or non-shader files.
constexpr std::uintmax_t kMaxSpirvFileBytes = 16 * 1024 * 1024;

[[nodiscard]] std::vector<char> readBinaryFile(std::string const& path) {
    std::error_code ec;
    std::filesystem::path const p(path);
    if (!std::filesystem::is_regular_file(p, ec) || ec) {
        return {};
    }
    std::uintmax_t const sz = std::filesystem::file_size(p, ec);
    if (ec || sz == 0 || sz > kMaxSpirvFileBytes) {
        return {};
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    std::vector<char> buf(static_cast<std::size_t>(sz));
    f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    if (!f || static_cast<std::size_t>(f.gcount()) != buf.size()) {
        return {};
    }
    return buf;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT /*severity*/,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    VkDebugUtilsMessengerCallbackDataEXT const* data,
    void* /*ud*/
) {
    if (data && data->pMessage) {
        fprintf(stderr, "Vulkan: %s\n", data->pMessage);
    }
    return VK_FALSE;
}

[[nodiscard]] VkResult createDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT* outMessenger) {
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (!fn) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debugCallback;
    return fn(instance, &ci, nullptr, outMessenger);
}

void destroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );
    if (fn) {
        fn(instance, messenger, nullptr);
    }
}

struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphicsFamily;
    std::optional<std::uint32_t> presentFamily;

    [[nodiscard]] bool complete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

[[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    QueueFamilyIndices idx;
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());
    for (std::uint32_t i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            idx.graphicsFamily = i;
        }
        VkBool32 present = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) {
            idx.presentFamily = i;
        }
        if (idx.complete()) {
            break;
        }
    }
    return idx;
}

[[nodiscard]] int deviceTypePreferenceRank(VkPhysicalDeviceType t) noexcept {
    switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 0;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 1;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 2;
    default:
        return 3;
    }
}

struct SuitablePhysicalDevice {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    QueueFamilyIndices queues{};
    int typeRank = 0;
    std::uint32_t enumeratedIndex = 0;
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

[[nodiscard]] SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice dev, VkSurfaceKHR surface) {
    SwapchainSupportDetails d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);
    std::uint32_t n = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &n, nullptr);
    if (n > 0) {
        d.formats.resize(n);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &n, d.formats.data());
    }
    n = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &n, nullptr);
    if (n > 0) {
        d.presentModes.resize(n);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &n, d.presentModes.data());
    }
    return d;
}

[[nodiscard]] VkSurfaceFormatKHR chooseSwapFormat(std::vector<VkSurfaceFormatKHR> const& formats) {
    for (auto const& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

[[nodiscard]] VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> const& modes) {
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

[[nodiscard]] VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR const& caps, int fbW, int fbH) {
    if (caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return caps.currentExtent;
    }
    VkExtent2D e{};
    e.width = static_cast<std::uint32_t>(
        std::clamp(fbW, static_cast<int>(caps.minImageExtent.width), static_cast<int>(caps.maxImageExtent.width))
    );
    e.height = static_cast<std::uint32_t>(
        std::clamp(fbH, static_cast<int>(caps.minImageExtent.height), static_cast<int>(caps.maxImageExtent.height))
    );
    return e;
}

[[nodiscard]] std::uint32_t findMemoryType(VkPhysicalDevice phys, std::uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (std::uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return std::numeric_limits<std::uint32_t>::max();
}

} // namespace

struct PushConstants {
    float model[16];
    float color[4];
    std::uint32_t drawFlags{};
};

struct GpuMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    std::uint32_t indexCount = 0;
};

struct VulkanRhiImpl {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapExtent{};
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipeline overlayPipeline = VK_NULL_HANDLE;
    std::uint32_t fullscreenQuadMesh = std::numeric_limits<std::uint32_t>::max();
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformMapped;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;

    std::vector<GpuMesh> meshes;
    std::vector<std::uint32_t> vertShaderCode;
    std::vector<std::uint32_t> fragShaderCode;

    std::uint32_t graphicsFamily = 0;
    std::uint32_t presentFamily = 0;
    std::uint32_t currentFrame = 0;

    float clearR = 0.08f;
    float clearG = 0.09f;
    float clearB = 0.12f;
    float clearA = 1.f;

    std::string shaderDirectory;

    static constexpr int kMaxFramesInFlight = 2;
};

[[nodiscard]] bool copyValidatedSpirvToImpl(
    VulkanRhiImpl& d,
    std::span<std::uint8_t const> vertBytes,
    std::span<std::uint8_t const> fragBytes
) noexcept {
    constexpr std::size_t kMaxBytes = static_cast<std::size_t>(16) * 1024 * 1024;
    if (vertBytes.empty() || fragBytes.empty() || vertBytes.size() % 4 != 0 || fragBytes.size() % 4 != 0) {
        return false;
    }
    if (vertBytes.size() > kMaxBytes || fragBytes.size() > kMaxBytes) {
        return false;
    }
    d.vertShaderCode.resize(vertBytes.size() / 4);
    d.fragShaderCode.resize(fragBytes.size() / 4);
    std::memcpy(d.vertShaderCode.data(), vertBytes.data(), vertBytes.size());
    std::memcpy(d.fragShaderCode.data(), fragBytes.data(), fragBytes.size());
    return true;
}

void destroySwapchainOnly(VulkanRhiImpl& d) {
    if (d.device == VK_NULL_HANDLE) {
        return;
    }
    for (auto fb : d.framebuffers) {
        vkDestroyFramebuffer(d.device, fb, nullptr);
    }
    d.framebuffers.clear();
    for (auto v : d.swapchainImageViews) {
        vkDestroyImageView(d.device, v, nullptr);
    }
    d.swapchainImageViews.clear();
    d.swapchainImages.clear();
    if (d.depthView) {
        vkDestroyImageView(d.device, d.depthView, nullptr);
        d.depthView = VK_NULL_HANDLE;
    }
    if (d.depthImage) {
        vkDestroyImage(d.device, d.depthImage, nullptr);
        d.depthImage = VK_NULL_HANDLE;
    }
    if (d.depthMemory) {
        vkFreeMemory(d.device, d.depthMemory, nullptr);
        d.depthMemory = VK_NULL_HANDLE;
    }
    if (d.swapchain) {
        vkDestroySwapchainKHR(d.device, d.swapchain, nullptr);
        d.swapchain = VK_NULL_HANDLE;
    }
}

[[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice dev) {
    std::uint32_t n = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> ext(n);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, ext.data());
    std::set<std::string> names;
    for (auto const& e : ext) {
        names.insert(e.extensionName);
    }
    if (names.count(VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
        return false;
    }
#if defined(__APPLE__)
    if (names.count(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
        return false;
    }
#endif
    return true;
}

[[nodiscard]] VkShaderModule makeShaderModule(VkDevice device, std::vector<std::uint32_t> const& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(std::uint32_t);
    ci.pCode = code.data();
    VkShaderModule m{};
    if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return m;
}

[[nodiscard]] VkFormat findDepthFormat(VkPhysicalDevice phys) {
    std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat f : candidates) {
        VkFormatProperties p;
        vkGetPhysicalDeviceFormatProperties(phys, f, &p);
        if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return f;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

bool createImage(
    VulkanRhiImpl& impl,
    std::uint32_t w,
    std::uint32_t h,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memProps,
    VkImage& image,
    VkDeviceMemory& memory
) {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.extent.width = w;
    ci.extent.height = h;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.format = format;
    ci.tiling = tiling;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.usage = usage;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(impl.device, &ci, nullptr, &image) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(impl.device, image, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(impl.physicalDevice, req.memoryTypeBits, memProps);
    if (ai.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max()) {
        vkDestroyImage(impl.device, image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }
    if (vkAllocateMemory(impl.device, &ai, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(impl.device, image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(impl.device, image, memory, 0);
    return true;
}

[[nodiscard]] VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = format;
    ci.subresourceRange.aspectMask = aspect;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;
    VkImageView v{};
    vkCreateImageView(device, &ci, nullptr, &v);
    return v;
}

bool createBuffer(
    VulkanRhiImpl& impl,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props,
    VkBuffer& buffer,
    VkDeviceMemory& memory
) {
    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(impl.device, &ci, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(impl.device, buffer, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(impl.physicalDevice, req.memoryTypeBits, props);
    if (ai.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max()) {
        vkDestroyBuffer(impl.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }
    if (vkAllocateMemory(impl.device, &ai, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(impl.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }
    vkBindBufferMemory(impl.device, buffer, memory, 0);
    return true;
}

void copyBuffer(VulkanRhiImpl& impl, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = impl.commandPool;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb{};
    vkAllocateCommandBuffers(impl.device, &ai, &cb);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cb, src, dst, 1, &region);
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(impl.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(impl.graphicsQueue);
    vkFreeCommandBuffers(impl.device, impl.commandPool, 1, &cb);
}

bool createSwapchainFull(VulkanRhiImpl& impl, platform::Window& window);

bool recreateSwapchain(VulkanRhiImpl& impl, platform::Window& window) {
    int w = 0, h = 0;
    window.getFramebufferSize(&w, &h);
    while (w == 0 || h == 0) {
        window.getFramebufferSize(&w, &h);
        window.pollEvents();
    }
    vkDeviceWaitIdle(impl.device);
    destroySwapchainOnly(impl);
    if (impl.overlayPipeline) {
        vkDestroyPipeline(impl.device, impl.overlayPipeline, nullptr);
        impl.overlayPipeline = VK_NULL_HANDLE;
    }
    if (impl.graphicsPipeline) {
        vkDestroyPipeline(impl.device, impl.graphicsPipeline, nullptr);
        impl.graphicsPipeline = VK_NULL_HANDLE;
    }
    if (impl.pipelineLayout) {
        vkDestroyPipelineLayout(impl.device, impl.pipelineLayout, nullptr);
        impl.pipelineLayout = VK_NULL_HANDLE;
    }
    if (impl.renderPass) {
        vkDestroyRenderPass(impl.device, impl.renderPass, nullptr);
        impl.renderPass = VK_NULL_HANDLE;
    }
    return createSwapchainFull(impl, window);
}

bool createRenderPassAndPipeline(VulkanRhiImpl& impl);

bool createSwapchainFull(VulkanRhiImpl& impl, platform::Window& window) {
    if (!impl.commandBuffers.empty() && impl.commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(
            impl.device,
            impl.commandPool,
            static_cast<std::uint32_t>(impl.commandBuffers.size()),
            impl.commandBuffers.data()
        );
        impl.commandBuffers.clear();
    }

    SwapchainSupportDetails sup = querySwapchainSupport(impl.physicalDevice, impl.surface);
    if (sup.formats.empty() || sup.presentModes.empty()) {
        return false;
    }
    VkSurfaceFormatKHR fmt = chooseSwapFormat(sup.formats);
    VkPresentModeKHR presentMode = choosePresentMode(sup.presentModes);
    int fbW = 0, fbH = 0;
    window.getFramebufferSize(&fbW, &fbH);
    VkExtent2D extent = chooseSwapExtent(sup.capabilities, fbW, fbH);
    std::uint32_t imageCount = sup.capabilities.minImageCount + 1;
    if (sup.capabilities.maxImageCount > 0 && imageCount > sup.capabilities.maxImageCount) {
        imageCount = sup.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = impl.surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = fmt.format;
    ci.imageColorSpace = fmt.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    std::uint32_t qf[] = {impl.graphicsFamily, impl.presentFamily};
    if (impl.graphicsFamily != impl.presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = qf;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    ci.preTransform = sup.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(impl.device, &ci, nullptr, &impl.swapchain) != VK_SUCCESS) {
        return false;
    }
    std::uint32_t n = 0;
    vkGetSwapchainImagesKHR(impl.device, impl.swapchain, &n, nullptr);
    impl.swapchainImages.resize(n);
    vkGetSwapchainImagesKHR(impl.device, impl.swapchain, &n, impl.swapchainImages.data());
    impl.swapchainFormat = fmt.format;
    impl.swapExtent = extent;
    impl.swapchainImageViews.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        impl.swapchainImageViews[i] = createImageView(impl.device, impl.swapchainImages[i], fmt.format, VK_IMAGE_ASPECT_COLOR_BIT);
        if (!impl.swapchainImageViews[i]) {
            return false;
        }
    }
    impl.depthFormat = findDepthFormat(impl.physicalDevice);
    if (impl.depthFormat == VK_FORMAT_UNDEFINED) {
        return false;
    }
    if (!createImage(
            impl,
            extent.width,
            extent.height,
            impl.depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            impl.depthImage,
            impl.depthMemory
        )) {
        return false;
    }
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (impl.depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || impl.depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    impl.depthView = createImageView(impl.device, impl.depthImage, impl.depthFormat, aspect);
    if (!impl.depthView) {
        return false;
    }
    if (!createRenderPassAndPipeline(impl)) {
        return false;
    }
    impl.framebuffers.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::array<VkImageView, 2> attachments = {impl.swapchainImageViews[i], impl.depthView};
        VkFramebufferCreateInfo fbi{};
        fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass = impl.renderPass;
        fbi.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        fbi.pAttachments = attachments.data();
        fbi.width = extent.width;
        fbi.height = extent.height;
        fbi.layers = 1;
        if (vkCreateFramebuffer(impl.device, &fbi, nullptr, &impl.framebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }
    impl.commandBuffers.resize(n);
    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = impl.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = static_cast<std::uint32_t>(n);
    if (vkAllocateCommandBuffers(impl.device, &cbai, impl.commandBuffers.data()) != VK_SUCCESS) {
        return false;
    }
    impl.imagesInFlight.assign(n, VK_NULL_HANDLE);
    return true;
}

bool createRenderPassAndPipeline(VulkanRhiImpl& impl) {
    VkAttachmentDescription color{};
    color.format = impl.swapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = impl.depthFormat;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference cref{};
    cref.attachment = 0;
    cref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference dref{};
    dref.attachment = 1;
    dref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &cref;
    sub.pDepthStencilAttachment = &dref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> atts = {color, depth};
    VkRenderPassCreateInfo rpi{};
    rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpi.attachmentCount = static_cast<std::uint32_t>(atts.size());
    rpi.pAttachments = atts.data();
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sub;
    rpi.dependencyCount = 1;
    rpi.pDependencies = &dep;
    if (vkCreateRenderPass(impl.device, &rpi, nullptr, &impl.renderPass) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo dsl{};
    dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl.bindingCount = 1;
    dsl.pBindings = &ubo;
    if (vkCreateDescriptorSetLayout(impl.device, &dsl, nullptr, &impl.descriptorSetLayout) != VK_SUCCESS) {
        return false;
    }

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &impl.descriptorSetLayout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(impl.device, &pli, nullptr, &impl.pipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VkShaderModule vert = makeShaderModule(impl.device, impl.vertShaderCode);
    VkShaderModule frag = makeShaderModule(impl.device, impl.fragShaderCode);
    if (!vert || !frag) {
        if (vert) {
            vkDestroyShaderModule(impl.device, vert, nullptr);
        }
        if (frag) {
            vkDestroyShaderModule(impl.device, frag, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = vert;
    vs.pName = "main";
    VkPipelineShaderStageCreateInfo fs{};
    fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = frag;
    fs.pName = "main";
    VkPipelineShaderStageCreateInfo stages[] = {vs, fs};

    VkVertexInputBindingDescription bind{};
    bind.binding = 0;
    bind.stride = sizeof(marble::render::VulkanRhi::Vertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 3> attrs{};
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(marble::render::VulkanRhi::Vertex, px);
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(marble::render::VulkanRhi::Vertex, nx);
    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = offsetof(marble::render::VulkanRhi::Vertex, cr);

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{};
    vp.x = 0.f;
    vp.y = 0.f;
    vp.width = static_cast<float>(impl.swapExtent.width);
    vp.height = static_cast<float>(impl.swapExtent.height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = impl.swapExtent;
    VkPipelineViewportStateCreateInfo vps{};
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1;
    vps.pViewports = &vp;
    vps.scissorCount = 1;
    vps.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.f;
    // Procedural garden meshes (discs, cylinders, subdivided icosa) mix winding with the dynamic
    // viewport Y-flip; back-face cull was culling outward faces and exposing “interiors”. Draw both
    // sides — depth test keeps outside views correct for closed solids at modest overdraw cost.
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsMain{};
    dsMain.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsMain.depthTestEnable = VK_TRUE;
    dsMain.depthWriteEnable = VK_TRUE;
    dsMain.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineDepthStencilStateCreateInfo dsOverlay{};
    dsOverlay.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsOverlay.depthTestEnable = VK_TRUE;
    dsOverlay.depthWriteEnable = VK_FALSE;
    dsOverlay.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    std::array<VkDynamicState, 2> dyn = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{};
    dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dy.dynamicStateCount = static_cast<std::uint32_t>(dyn.size());
    dy.pDynamicStates = dyn.data();

    VkGraphicsPipelineCreateInfo gpi{};
    gpi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpi.stageCount = 2;
    gpi.pStages = stages;
    gpi.pVertexInputState = &vi;
    gpi.pInputAssemblyState = &ia;
    gpi.pViewportState = &vps;
    gpi.pRasterizationState = &rs;
    gpi.pMultisampleState = &ms;
    gpi.pDepthStencilState = &dsMain;
    gpi.pColorBlendState = &cb;
    gpi.pDynamicState = &dy;
    gpi.layout = impl.pipelineLayout;
    gpi.renderPass = impl.renderPass;
    gpi.subpass = 0;

    if (vkCreateGraphicsPipelines(impl.device, VK_NULL_HANDLE, 1, &gpi, nullptr, &impl.graphicsPipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(impl.device, vert, nullptr);
        vkDestroyShaderModule(impl.device, frag, nullptr);
        return false;
    }

    VkPipelineRasterizationStateCreateInfo rsOverlay = rs;
    rsOverlay.cullMode = VK_CULL_MODE_NONE;
    gpi.pRasterizationState = &rsOverlay;
    gpi.pDepthStencilState = &dsOverlay;
    if (vkCreateGraphicsPipelines(impl.device, VK_NULL_HANDLE, 1, &gpi, nullptr, &impl.overlayPipeline) != VK_SUCCESS) {
        vkDestroyPipeline(impl.device, impl.graphicsPipeline, nullptr);
        impl.graphicsPipeline = VK_NULL_HANDLE;
        vkDestroyShaderModule(impl.device, vert, nullptr);
        vkDestroyShaderModule(impl.device, frag, nullptr);
        return false;
    }

    vkDestroyShaderModule(impl.device, vert, nullptr);
    vkDestroyShaderModule(impl.device, frag, nullptr);
    return true;
}

[[nodiscard]] bool completeVulkanInitAfterSpirv(
    VulkanRhiImpl& d,
    platform::Window& window,
    char const* appName,
    std::optional<std::uint32_t> physicalDeviceIndex
) {
    std::uint32_t glfwCount = 0;
    char const* const* glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<char const*> extensions(glfwExt, glfwExt + glfwCount);
#if MARBLE_VK_ENABLE_VALIDATION
    char const* const debugExt = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    extensions.push_back(debugExt);
#endif

#if defined(__APPLE__)
    {
        bool havePortEnum = false;
        for (char const* e : extensions) {
            if (std::strcmp(e, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                havePortEnum = true;
                break;
            }
        }
        if (!havePortEnum) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        }
    }
#endif

#if MARBLE_VK_ENABLE_VALIDATION
    char const* layers[] = {"VK_LAYER_KHRONOS_validation"};
#endif

    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = appName;
    ai.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.pEngineName = "Marble";
    ai.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    ai.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &ai;
    ici.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();
#if defined(__APPLE__)
    ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
#if MARBLE_VK_ENABLE_VALIDATION
    ici.enabledLayerCount = 1;
    ici.ppEnabledLayerNames = layers;
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCi{};
    debugMessengerCi.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerCi.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerCi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugMessengerCi.pfnUserCallback = debugCallback;
    ici.pNext = &debugMessengerCi;
#else
    ici.enabledLayerCount = 0;
#endif
    if (vkCreateInstance(&ici, nullptr, &d.instance) != VK_SUCCESS) {
        return false;
    }
#if MARBLE_VK_ENABLE_VALIDATION
    (void)createDebugMessenger(d.instance, &d.debugMessenger);
#endif
    if (!window.createVulkanSurface(d.instance, &d.surface)) {
        return false;
    }

    std::uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(d.instance, &devCount, nullptr);
    if (devCount == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(d.instance, &devCount, devices.data());
    std::vector<SuitablePhysicalDevice> suitable;
    suitable.reserve(devCount);
    for (std::uint32_t i = 0; i < devCount; ++i) {
        VkPhysicalDevice const pd = devices[i];
        if (!checkDeviceExtensionSupport(pd)) {
            continue;
        }
        QueueFamilyIndices q = findQueueFamilies(pd, d.surface);
        if (!q.complete()) {
            continue;
        }
        SwapchainSupportDetails sup = querySwapchainSupport(pd, d.surface);
        if (sup.formats.empty() || sup.presentModes.empty()) {
            continue;
        }
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pd, &props);
        SuitablePhysicalDevice cand{};
        cand.device = pd;
        cand.queues = q;
        cand.typeRank = deviceTypePreferenceRank(props.deviceType);
        cand.enumeratedIndex = i;
        suitable.push_back(cand);
    }
    if (suitable.empty()) {
        return false;
    }
    std::stable_sort(suitable.begin(), suitable.end(), [](SuitablePhysicalDevice const& a, SuitablePhysicalDevice const& b) {
        if (a.typeRank != b.typeRank) {
            return a.typeRank < b.typeRank;
        }
        return a.enumeratedIndex < b.enumeratedIndex;
    });

    std::size_t pick = 0;
    if (physicalDeviceIndex.has_value()) {
        if (*physicalDeviceIndex >= suitable.size()) {
            return false;
        }
        pick = static_cast<std::size_t>(*physicalDeviceIndex);
    }

    SuitablePhysicalDevice const& chosen = suitable[pick];
    d.physicalDevice = chosen.device;
    d.graphicsFamily = *chosen.queues.graphicsFamily;
    d.presentFamily = *chosen.queues.presentFamily;

    float const priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    std::set<std::uint32_t> uniq = {d.graphicsFamily, d.presentFamily};
    for (std::uint32_t qf : uniq) {
        VkDeviceQueueCreateInfo q{};
        q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q.queueFamilyIndex = qf;
        q.queueCount = 1;
        q.pQueuePriorities = &priority;
        qcis.push_back(q);
    }
#if defined(__APPLE__)
    char const* devExt[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
    };
    std::uint32_t const devExtCount = 2;
#else
    char const* devExt[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::uint32_t const devExtCount = 1;
#endif
    VkDeviceCreateInfo deviceCi{};
    deviceCi.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCi.queueCreateInfoCount = static_cast<std::uint32_t>(qcis.size());
    deviceCi.pQueueCreateInfos = qcis.data();
    deviceCi.enabledExtensionCount = devExtCount;
    deviceCi.ppEnabledExtensionNames = devExt;
    if (vkCreateDevice(d.physicalDevice, &deviceCi, nullptr, &d.device) != VK_SUCCESS) {
        return false;
    }
    vkGetDeviceQueue(d.device, d.graphicsFamily, 0, &d.graphicsQueue);
    vkGetDeviceQueue(d.device, d.presentFamily, 0, &d.presentQueue);

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = d.graphicsFamily;
    if (vkCreateCommandPool(d.device, &cpci, nullptr, &d.commandPool) != VK_SUCCESS) {
        return false;
    }

    if (!createSwapchainFull(d, window)) {
        return false;
    }

    VkDeviceSize uboAlign = sizeof(math::Mat4);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(d.physicalDevice, &props);
    VkDeviceSize aligned = uboAlign;
    if (props.limits.minUniformBufferOffsetAlignment > 0) {
        VkDeviceSize a = props.limits.minUniformBufferOffsetAlignment;
        aligned = (sizeof(math::Mat4) + a - 1) / a * a;
    }
    VkDeviceSize uboSize = aligned;

    d.uniformBuffers.resize(VulkanRhiImpl::kMaxFramesInFlight);
    d.uniformBuffersMemory.resize(VulkanRhiImpl::kMaxFramesInFlight);
    d.uniformMapped.resize(VulkanRhiImpl::kMaxFramesInFlight);
    for (int i = 0; i < VulkanRhiImpl::kMaxFramesInFlight; ++i) {
        if (!createBuffer(
                d,
                uboSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                d.uniformBuffers[i],
                d.uniformBuffersMemory[i]
            )) {
            return false;
        }
        if (vkMapMemory(d.device, d.uniformBuffersMemory[i], 0, uboSize, 0, &d.uniformMapped[i]) != VK_SUCCESS) {
            return false;
        }
    }

    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<std::uint32_t>(VulkanRhiImpl::kMaxFramesInFlight);
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    dpci.pPoolSizes = poolSizes.data();
    dpci.maxSets = static_cast<std::uint32_t>(VulkanRhiImpl::kMaxFramesInFlight);
    if (vkCreateDescriptorPool(d.device, &dpci, nullptr, &d.descriptorPool) != VK_SUCCESS) {
        return false;
    }

    std::vector<VkDescriptorSetLayout> layouts(VulkanRhiImpl::kMaxFramesInFlight, d.descriptorSetLayout);
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = d.descriptorPool;
    dsai.descriptorSetCount = static_cast<std::uint32_t>(VulkanRhiImpl::kMaxFramesInFlight);
    dsai.pSetLayouts = layouts.data();
    d.descriptorSets.resize(VulkanRhiImpl::kMaxFramesInFlight);
    if (vkAllocateDescriptorSets(d.device, &dsai, d.descriptorSets.data()) != VK_SUCCESS) {
        return false;
    }
    for (int i = 0; i < VulkanRhiImpl::kMaxFramesInFlight; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = d.uniformBuffers[i];
        bi.offset = 0;
        bi.range = sizeof(math::Mat4);
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = d.descriptorSets[i];
        w.dstBinding = 0;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(d.device, 1, &w, 0, nullptr);
    }

    d.imageAvailableSemaphores.resize(VulkanRhiImpl::kMaxFramesInFlight);
    d.renderFinishedSemaphores.resize(VulkanRhiImpl::kMaxFramesInFlight);
    d.inFlightFences.resize(VulkanRhiImpl::kMaxFramesInFlight);
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < VulkanRhiImpl::kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(d.device, &sci, nullptr, &d.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(d.device, &sci, nullptr, &d.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(d.device, &fci, nullptr, &d.inFlightFences[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

VulkanRhi::VulkanRhi() : impl_(std::make_unique<VulkanRhiImpl>()) {}

VulkanRhi::~VulkanRhi() {
    shutdown();
}

VulkanRhi::VulkanRhi(VulkanRhi&& o) noexcept : impl_(std::move(o.impl_)) {
    if (!o.impl_) {
        o.impl_ = std::make_unique<VulkanRhiImpl>();
    }
}

VulkanRhi& VulkanRhi::operator=(VulkanRhi&& o) noexcept {
    if (this != &o) {
        shutdown();
        impl_ = std::move(o.impl_);
        if (!o.impl_) {
            o.impl_ = std::make_unique<VulkanRhiImpl>();
        }
    }
    return *this;
}

bool VulkanRhi::initialized() const {
    return impl_ && impl_->instance != VK_NULL_HANDLE;
}

void VulkanRhi::shutdown() {
    if (!impl_) {
        return;
    }
    VulkanRhiImpl& d = *impl_;
    if (d.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(d.device);
        for (auto& m : d.meshes) {
            if (m.indexBuffer) {
                vkDestroyBuffer(d.device, m.indexBuffer, nullptr);
            }
            if (m.vertexBuffer) {
                vkDestroyBuffer(d.device, m.vertexBuffer, nullptr);
            }
            if (m.indexMemory) {
                vkFreeMemory(d.device, m.indexMemory, nullptr);
            }
            if (m.vertexMemory) {
                vkFreeMemory(d.device, m.vertexMemory, nullptr);
            }
        }
        d.meshes.clear();
        for (std::size_t i = 0; i < d.uniformBuffers.size(); ++i) {
            if (d.uniformMapped[i]) {
                vkUnmapMemory(d.device, d.uniformBuffersMemory[i]);
                d.uniformMapped[i] = nullptr;
            }
            if (d.uniformBuffers[i]) {
                vkDestroyBuffer(d.device, d.uniformBuffers[i], nullptr);
            }
            if (d.uniformBuffersMemory[i]) {
                vkFreeMemory(d.device, d.uniformBuffersMemory[i], nullptr);
            }
        }
        d.uniformBuffers.clear();
        d.uniformBuffersMemory.clear();
        d.uniformMapped.clear();
        for (auto s : d.imageAvailableSemaphores) {
            vkDestroySemaphore(d.device, s, nullptr);
        }
        for (auto s : d.renderFinishedSemaphores) {
            vkDestroySemaphore(d.device, s, nullptr);
        }
        for (auto f : d.inFlightFences) {
            vkDestroyFence(d.device, f, nullptr);
        }
        d.imageAvailableSemaphores.clear();
        d.renderFinishedSemaphores.clear();
        d.inFlightFences.clear();
        d.imagesInFlight.clear();
        if (d.descriptorPool) {
            vkDestroyDescriptorPool(d.device, d.descriptorPool, nullptr);
            d.descriptorPool = VK_NULL_HANDLE;
        }
        if (d.descriptorSetLayout) {
            vkDestroyDescriptorSetLayout(d.device, d.descriptorSetLayout, nullptr);
            d.descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (d.commandPool) {
            vkDestroyCommandPool(d.device, d.commandPool, nullptr);
            d.commandPool = VK_NULL_HANDLE;
        }
        d.commandBuffers.clear();
        destroySwapchainOnly(d);
        if (d.overlayPipeline) {
            vkDestroyPipeline(d.device, d.overlayPipeline, nullptr);
            d.overlayPipeline = VK_NULL_HANDLE;
        }
        if (d.graphicsPipeline) {
            vkDestroyPipeline(d.device, d.graphicsPipeline, nullptr);
            d.graphicsPipeline = VK_NULL_HANDLE;
        }
        if (d.pipelineLayout) {
            vkDestroyPipelineLayout(d.device, d.pipelineLayout, nullptr);
            d.pipelineLayout = VK_NULL_HANDLE;
        }
        if (d.renderPass) {
            vkDestroyRenderPass(d.device, d.renderPass, nullptr);
            d.renderPass = VK_NULL_HANDLE;
        }
        vkDestroyDevice(d.device, nullptr);
        d.device = VK_NULL_HANDLE;
    }
    if (d.surface) {
        vkDestroySurfaceKHR(d.instance, d.surface, nullptr);
        d.surface = VK_NULL_HANDLE;
    }
#if MARBLE_VK_ENABLE_VALIDATION
    if (d.debugMessenger) {
        destroyDebugMessenger(d.instance, d.debugMessenger);
        d.debugMessenger = VK_NULL_HANDLE;
    }
#endif
    if (d.instance) {
        vkDestroyInstance(d.instance, nullptr);
        d.instance = VK_NULL_HANDLE;
    }
}

void VulkanRhi::setClearColor(float r, float g, float b, float a) {
    if (!impl_) {
        return;
    }
    impl_->clearR = r;
    impl_->clearG = g;
    impl_->clearB = b;
    impl_->clearA = a;
}

bool VulkanRhi::init(
    platform::Window& window,
    char const* appName,
    std::string shaderDirectory,
    std::optional<std::uint32_t> physicalDeviceIndex
) {
    shutdown();
    impl_ = std::make_unique<VulkanRhiImpl>();
    VulkanRhiImpl& d = *impl_;
    d.shaderDirectory = std::move(shaderDirectory);

    std::string const vertPath = d.shaderDirectory + "/mesh.vert.spv";
    std::string const fragPath = d.shaderDirectory + "/mesh.frag.spv";
    auto const vertFile = readBinaryFile(vertPath);
    auto const fragFile = readBinaryFile(fragPath);
    std::span<std::uint8_t const> const vertSpan(
        reinterpret_cast<std::uint8_t const*>(vertFile.data()), vertFile.size());
    std::span<std::uint8_t const> const fragSpan(
        reinterpret_cast<std::uint8_t const*>(fragFile.data()), fragFile.size());
    if (!copyValidatedSpirvToImpl(d, vertSpan, fragSpan)) {
        return false;
    }
    if (!completeVulkanInitAfterSpirv(d, window, appName, physicalDeviceIndex)) {
        shutdown();
        return false;
    }
    return true;
}

bool VulkanRhi::initFromSpirvBytes(
    platform::Window& window,
    char const* appName,
    std::span<std::uint8_t const> vertSpirv,
    std::span<std::uint8_t const> fragSpirv,
    std::optional<std::uint32_t> physicalDeviceIndex
) {
    shutdown();
    impl_ = std::make_unique<VulkanRhiImpl>();
    VulkanRhiImpl& d = *impl_;
    d.shaderDirectory.clear();
    if (!copyValidatedSpirvToImpl(d, vertSpirv, fragSpirv)) {
        return false;
    }
    if (!completeVulkanInitAfterSpirv(d, window, appName, physicalDeviceIndex)) {
        shutdown();
        return false;
    }
    return true;
}

std::uint32_t VulkanRhi::uploadMesh(std::span<Vertex const> vertices, std::span<std::uint32_t const> indices) {
    constexpr std::uint32_t kBad = std::numeric_limits<std::uint32_t>::max();
    if (!impl_ || impl_->device == VK_NULL_HANDLE || vertices.empty() || indices.empty()) {
        return kBad;
    }
    VulkanRhiImpl& d = *impl_;

    VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize ibSize = sizeof(std::uint32_t) * indices.size();

    VkBuffer stagingVb{};
    VkDeviceMemory stagingVbMem{};
    VkBuffer stagingIb{};
    VkDeviceMemory stagingIbMem{};
    if (!createBuffer(
            d,
            vbSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingVb,
            stagingVbMem
        )) {
        return kBad;
    }
    void* data = nullptr;
    vkMapMemory(d.device, stagingVbMem, 0, vbSize, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<std::size_t>(vbSize));
    vkUnmapMemory(d.device, stagingVbMem);

    if (!createBuffer(
            d,
            ibSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingIb,
            stagingIbMem
        )) {
        vkDestroyBuffer(d.device, stagingVb, nullptr);
        vkFreeMemory(d.device, stagingVbMem, nullptr);
        return kBad;
    }
    vkMapMemory(d.device, stagingIbMem, 0, ibSize, 0, &data);
    std::memcpy(data, indices.data(), static_cast<std::size_t>(ibSize));
    vkUnmapMemory(d.device, stagingIbMem);

    GpuMesh mesh{};
    if (!createBuffer(
            d,
            vbSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.vertexBuffer,
            mesh.vertexMemory
        )) {
        vkDestroyBuffer(d.device, stagingVb, nullptr);
        vkFreeMemory(d.device, stagingVbMem, nullptr);
        vkDestroyBuffer(d.device, stagingIb, nullptr);
        vkFreeMemory(d.device, stagingIbMem, nullptr);
        return kBad;
    }
    if (!createBuffer(
            d,
            ibSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.indexBuffer,
            mesh.indexMemory
        )) {
        vkDestroyBuffer(d.device, mesh.vertexBuffer, nullptr);
        vkFreeMemory(d.device, mesh.vertexMemory, nullptr);
        vkDestroyBuffer(d.device, stagingVb, nullptr);
        vkFreeMemory(d.device, stagingVbMem, nullptr);
        vkDestroyBuffer(d.device, stagingIb, nullptr);
        vkFreeMemory(d.device, stagingIbMem, nullptr);
        return kBad;
    }
    copyBuffer(d, stagingVb, mesh.vertexBuffer, vbSize);
    copyBuffer(d, stagingIb, mesh.indexBuffer, ibSize);
    vkDestroyBuffer(d.device, stagingVb, nullptr);
    vkFreeMemory(d.device, stagingVbMem, nullptr);
    vkDestroyBuffer(d.device, stagingIb, nullptr);
    vkFreeMemory(d.device, stagingIbMem, nullptr);

    mesh.indexCount = static_cast<std::uint32_t>(indices.size());
    d.meshes.push_back(mesh);
    return static_cast<std::uint32_t>(d.meshes.size() - 1);
}

bool VulkanRhi::replaceMesh(
    std::uint32_t meshIndex,
    std::span<Vertex const> vertices,
    std::span<std::uint32_t const> indices
) {
    if (!impl_ || impl_->device == VK_NULL_HANDLE || vertices.empty() || indices.empty() ||
        meshIndex >= impl_->meshes.size()) {
        return false;
    }
    VulkanRhiImpl& d = *impl_;
    vkDeviceWaitIdle(d.device);
    GpuMesh& old = d.meshes[meshIndex];
    if (old.indexBuffer) {
        vkDestroyBuffer(d.device, old.indexBuffer, nullptr);
    }
    if (old.vertexBuffer) {
        vkDestroyBuffer(d.device, old.vertexBuffer, nullptr);
    }
    if (old.indexMemory) {
        vkFreeMemory(d.device, old.indexMemory, nullptr);
    }
    if (old.vertexMemory) {
        vkFreeMemory(d.device, old.vertexMemory, nullptr);
    }
    old = {};

    VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize ibSize = sizeof(std::uint32_t) * indices.size();

    VkBuffer stagingVb{};
    VkDeviceMemory stagingVbMem{};
    VkBuffer stagingIb{};
    VkDeviceMemory stagingIbMem{};
    if (!createBuffer(
            d,
            vbSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingVb,
            stagingVbMem
        )) {
        return false;
    }
    void* data = nullptr;
    vkMapMemory(d.device, stagingVbMem, 0, vbSize, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<std::size_t>(vbSize));
    vkUnmapMemory(d.device, stagingVbMem);

    if (!createBuffer(
            d,
            ibSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingIb,
            stagingIbMem
        )) {
        vkDestroyBuffer(d.device, stagingVb, nullptr);
        vkFreeMemory(d.device, stagingVbMem, nullptr);
        return false;
    }
    vkMapMemory(d.device, stagingIbMem, 0, ibSize, 0, &data);
    std::memcpy(data, indices.data(), static_cast<std::size_t>(ibSize));
    vkUnmapMemory(d.device, stagingIbMem);

    GpuMesh mesh{};
    if (!createBuffer(
            d,
            vbSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.vertexBuffer,
            mesh.vertexMemory
        )) {
        vkDestroyBuffer(d.device, stagingVb, nullptr);
        vkFreeMemory(d.device, stagingVbMem, nullptr);
        vkDestroyBuffer(d.device, stagingIb, nullptr);
        vkFreeMemory(d.device, stagingIbMem, nullptr);
        return false;
    }
    if (!createBuffer(
            d,
            ibSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            mesh.indexBuffer,
            mesh.indexMemory
        )) {
        vkDestroyBuffer(d.device, mesh.vertexBuffer, nullptr);
        vkFreeMemory(d.device, mesh.vertexMemory, nullptr);
        vkDestroyBuffer(d.device, stagingVb, nullptr);
        vkFreeMemory(d.device, stagingVbMem, nullptr);
        vkDestroyBuffer(d.device, stagingIb, nullptr);
        vkFreeMemory(d.device, stagingIbMem, nullptr);
        return false;
    }
    copyBuffer(d, stagingVb, mesh.vertexBuffer, vbSize);
    copyBuffer(d, stagingIb, mesh.indexBuffer, ibSize);
    vkDestroyBuffer(d.device, stagingVb, nullptr);
    vkFreeMemory(d.device, stagingVbMem, nullptr);
    vkDestroyBuffer(d.device, stagingIb, nullptr);
    vkFreeMemory(d.device, stagingIbMem, nullptr);

    mesh.indexCount = static_cast<std::uint32_t>(indices.size());
    d.meshes[meshIndex] = mesh;
    return true;
}

void VulkanRhi::ensureFullscreenQuadMesh() {
    if (!impl_) {
        return;
    }
    VulkanRhiImpl& d = *impl_;
    if (d.fullscreenQuadMesh != std::numeric_limits<std::uint32_t>::max()) {
        return;
    }
    std::array<Vertex, 4> verts{};
    verts[0] = Vertex{-1.f, -1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    verts[1] = Vertex{1.f, -1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    verts[2] = Vertex{1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    verts[3] = Vertex{-1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::array<std::uint32_t, 6> const idx = {0, 1, 2, 2, 3, 0};
    d.fullscreenQuadMesh = uploadMesh(std::span<Vertex const>(verts.data(), verts.size()), idx);
}

bool VulkanRhi::drawFrame(
    platform::Window& window,
    math::Mat4 const& viewProj,
    std::span<MeshDrawInstance const> draws,
    FrameOverlayTint const* overlayTint
) {
    if (!impl_ || impl_->device == VK_NULL_HANDLE) {
        return false;
    }
    VulkanRhiImpl& d = *impl_;

    if (window.consumeFramebufferResized()) {
        if (!recreateSwapchain(d, window)) {
            return false;
        }
    }

    vkWaitForFences(d.device, 1, &d.inFlightFences[d.currentFrame], VK_TRUE, UINT64_MAX);

    std::uint32_t imageIndex = 0;
    VkResult acq = vkAcquireNextImageKHR(
        d.device,
        d.swapchain,
        UINT64_MAX,
        d.imageAvailableSemaphores[d.currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );
    while (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!recreateSwapchain(d, window)) {
            return false;
        }
        acq = vkAcquireNextImageKHR(
            d.device,
            d.swapchain,
            UINT64_MAX,
            d.imageAvailableSemaphores[d.currentFrame],
            VK_NULL_HANDLE,
            &imageIndex
        );
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    if (d.imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(d.device, 1, &d.imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    d.imagesInFlight[imageIndex] = d.inFlightFences[d.currentFrame];

    std::memcpy(d.uniformMapped[d.currentFrame], &viewProj.m, sizeof(math::Mat4));

    vkResetFences(d.device, 1, &d.inFlightFences[d.currentFrame]);

    vkResetCommandBuffer(d.commandBuffers[imageIndex], 0);
    VkCommandBuffer cb = d.commandBuffers[imageIndex];

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cb, &bi);

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{d.clearR, d.clearG, d.clearB, d.clearA}};
    clears[1].depthStencil = {1.f, 0};
    VkRenderPassBeginInfo rpbi{};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = d.renderPass;
    rpbi.framebuffer = d.framebuffers[imageIndex];
    rpbi.renderArea.offset = {0, 0};
    rpbi.renderArea.extent = d.swapExtent;
    rpbi.clearValueCount = static_cast<std::uint32_t>(clears.size());
    rpbi.pClearValues = clears.data();
    vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, d.graphicsPipeline);

    VkViewport vp{};
    vp.x = 0.f;
    vp.y = static_cast<float>(d.swapExtent.height);
    vp.width = static_cast<float>(d.swapExtent.width);
    vp.height = -static_cast<float>(d.swapExtent.height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    vkCmdSetViewport(cb, 0, 1, &vp);
    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = d.swapExtent;
    vkCmdSetScissor(cb, 0, 1, &sc);

    vkCmdBindDescriptorSets(
        cb,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        d.pipelineLayout,
        0,
        1,
        &d.descriptorSets[d.currentFrame],
        0,
        nullptr
    );

    for (MeshDrawInstance const& dc : draws) {
        if (dc.meshIndex >= d.meshes.size()) {
            continue;
        }
        GpuMesh const& m = d.meshes[dc.meshIndex];
        PushConstants pc{};
        std::memcpy(pc.model, dc.model.m, sizeof(pc.model));
        pc.color[0] = dc.color.x;
        pc.color[1] = dc.color.y;
        pc.color[2] = dc.color.z;
        pc.color[3] = dc.colorAlpha;
        pc.drawFlags = dc.drawFlags;
        vkCmdPushConstants(cb, d.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
        VkBuffer vb = m.vertexBuffer;
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cb, m.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, m.indexCount, 1, 0, 0, 0);
    }

    if (overlayTint != nullptr && overlayTint->a > 0.f && d.overlayPipeline != VK_NULL_HANDLE) {
        ensureFullscreenQuadMesh();
        if (d.fullscreenQuadMesh < d.meshes.size()) {
            GpuMesh const& qm = d.meshes[d.fullscreenQuadMesh];
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, d.overlayPipeline);
            PushConstants opc{};
            math::Mat4 const id = math::Mat4::identity();
            std::memcpy(opc.model, id.m, sizeof(opc.model));
            opc.color[0] = overlayTint->r;
            opc.color[1] = overlayTint->g;
            opc.color[2] = overlayTint->b;
            opc.color[3] = overlayTint->a;
            opc.drawFlags = marble::render::kPcFlagClipSpace;
            vkCmdPushConstants(cb, d.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &opc);
            VkBuffer vb = qm.vertexBuffer;
            VkDeviceSize off = 0;
            vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
            vkCmdBindIndexBuffer(cb, qm.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cb, qm.indexCount, 1, 0, 0, 0);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, d.graphicsPipeline);
        }
    }

    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &d.imageAvailableSemaphores[d.currentFrame];
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &d.renderFinishedSemaphores[d.currentFrame];
    if (vkQueueSubmit(d.graphicsQueue, 1, &si, d.inFlightFences[d.currentFrame]) != VK_SUCCESS) {
        return false;
    }

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &d.renderFinishedSemaphores[d.currentFrame];
    pi.swapchainCount = 1;
    pi.pSwapchains = &d.swapchain;
    pi.pImageIndices = &imageIndex;
    VkResult pr = vkQueuePresentKHR(d.presentQueue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
        if (!recreateSwapchain(d, window)) {
            return false;
        }
    } else if (pr != VK_SUCCESS) {
        return false;
    }

    d.currentFrame = (d.currentFrame + 1) % VulkanRhiImpl::kMaxFramesInFlight;
    return true;
}

} // namespace marble::render
