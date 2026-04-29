#pragma once

#include <set>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>
#include <iostream>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "glfw_context.h"
#include "window.h"

#include "vulkan_surface.h"
#include "vulkan_instance.h"

class VulkanEngine {
public:
    _XCLASS_NAME(VulkanEngine);

    explicit VulkanEngine(const GlfwContext& glfw_context, Window& window, std::string_view app_name = "vulkan_engine");

    ~VulkanEngine();
    void destroy();

    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    VulkanEngine(VulkanEngine&&) = delete;
    VulkanEngine& operator=(VulkanEngine&&) = delete;

    void init();
    void init_vulkan();

    void run();

private:
    Window& m_window;
    VulkanInstance m_instance;
    VulkanSurface m_surface;

private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;

    const std::vector<const char*> m_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool is_complete() const {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

private:
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;

    VkFormat m_swapchain_image_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent{};

private:
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

private:
    void pick_physical_device();
    void create_logical_device();

    bool is_device_suitable(VkPhysicalDevice device) const;
    bool check_device_extension_support(VkPhysicalDevice device) const;
    QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const;

private:
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_swapchain_framebuffers;

private:
    void create_render_pass();
    void create_framebuffers();

private:
    SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device) const;

    VkSurfaceFormatKHR choose_swap_surface_format(
        const std::vector<VkSurfaceFormatKHR>& available_formats
    ) const;

    VkPresentModeKHR choose_swap_present_mode(
        const std::vector<VkPresentModeKHR>& available_present_modes
    ) const;

    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    void create_swapchain();
    void create_image_views();

private:
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    size_t m_current_frame = 0;

    std::vector<VkSemaphore> m_image_available_semaphores;
    std::vector<VkSemaphore> m_render_finished_semaphores;
    std::vector<VkFence> m_in_flight_fences;

private:
    void create_command_pool();
    void create_command_buffers();
    void create_sync_objects();

    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index);
    void draw_frame();
};
