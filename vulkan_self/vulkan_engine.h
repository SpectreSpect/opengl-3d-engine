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

#include "vulkan_instance.h"
#include "vulkan_surface.h"
#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"

class VulkanEngine {
public:
    _XCLASS_NAME(VulkanEngine);

    explicit VulkanEngine(
        const GlfwContext& glfw_context,
        Window& window,
        const QueueRequest& queue_request,
        std::string_view app_name = "vulkan_engine"
    );

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
    VulkanPhysicalDevice m_physical_device;
    VulkanDevice m_device;
    VulkanSwapchain m_swapchain;

    std::vector<VkImageView> m_swapchain_image_views;

private:
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_swapchain_framebuffers;

private:
    void create_render_pass();
    void create_framebuffers();

private:
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
