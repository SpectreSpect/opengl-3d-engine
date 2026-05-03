#pragma once

#include "vulkan_window.h"

#include <cstdint>
#include <imgui.h>
#include <vulkan/vulkan.h>


struct GLFWwindow;

namespace ui {

struct VulkanInitInfo {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    VkQueue queue = VK_NULL_HANDLE;

    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;

    uint32_t min_image_count = 2;
    uint32_t image_count = 2;

    VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    uint32_t subpass = 0;

    const VkAllocationCallbacks* allocator = nullptr;
    void (*check_vk_result_fn)(VkResult err) = nullptr;
};

void init(GLFWwindow* window, const VulkanInitInfo& info);
void init(VulkanWindow* window, VulkanEngine* engine);
void begin_frame();
void update_mouse_mode(VulkanWindow* window);
void end_frame(VkCommandBuffer command_buffer);
void set_min_image_count(uint32_t min_image_count);
void shutdown();

} // namespace ui