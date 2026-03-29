#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>

class CommandPool {
public:
    VkDevice* device = nullptr;
    VkPhysicalDevice* physical_device = nullptr;
    VkCommandPool pool = VK_NULL_HANDLE;

    uint32_t queue_family_id = 0;
    VkQueue queue = VK_NULL_HANDLE;

    CommandPool() = default;
    CommandPool(VkDevice& device, VkPhysicalDevice& physical_device, uint32_t queue_family_id, VkQueue queue);

    void create(VkDevice& device, VkPhysicalDevice& physical_device, uint32_t queue_family_id, VkQueue queue);
    void destroy();
};