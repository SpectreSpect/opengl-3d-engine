#pragma once

#include <vulkan/vulkan.h>
#include "engine3d.h"
#include "window.h"


#include <GLFW/glfw3.h>

#include <optional>
#include <set>
#include <string>
#include <vector>
#include "vulkan_window.h"

class VulkanEngine : public Engine3D {
public:
    VulkanEngine();
    ~VulkanEngine();

    int init() override;
    void set_window(Window* window) override;
    void set_vulkan_window(VulkanWindow* window);
    void poll_events() override;
    void enable_depth_test() override;

    virtual void begin_frame(const glm::vec4& clear_color) override;
    virtual void end_frame() override;
    virtual void on_framebuffer_resized(int w, int h) override;

    void recreate_swapchain();
    void create_framebuffers();
    void create_command_buffers();
    void create_sync_objects();
    void destroy_swapchain_dependent_objects();
    void createRenderPass();
    void createCommandPool();
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, 
                                        VkImageTiling tiling, VkFormatFeatureFlags features);
    static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
    static bool hasStencilComponent(VkFormat format);
    static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, 
                            VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                            VkImage& image, VkDeviceMemory& imageMemory);
    static VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    static void createDepthResources(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkImage& depthImage, 
                                     VkDeviceMemory& depthImageMemory, VkImageView& depthImageView, VkFormat& depthFormat);

    static void vk_check(VkResult res, const char* what) {
        if (res != VK_SUCCESS) {
            throw std::runtime_error(std::string(what) + " failed with VkResult = " + std::to_string(res));
        }
    }
private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

public:
    VulkanWindow* m_window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    uint32_t currentImageIndex = 0;
    VkCommandBuffer currentCommandBuffer = VK_NULL_HANDLE;

    bool frameInProgress = false;
    bool framebufferResized = false;

private:
    void cleanup();

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Needed if you want actual window rendering:
    void createSwapchain();
    void createImageViews();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) const;
    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev) const;

    std::vector<const char*> getRequiredInstanceExtensions() const;
};