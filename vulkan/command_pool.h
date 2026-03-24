#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_utils.h"

class CommandPool {
public:
    VkDevice* device = nullptr;
    VkPhysicalDevice* physical_device = nullptr;
    VkCommandPool pool;

    uint32_t compute_queue_family_id;
    VkQueue compute_queue;

    CommandPool() = default;
    CommandPool(VkDevice& device, VkPhysicalDevice& physical_device);
    void create(VkDevice& device, VkPhysicalDevice& physical_device);
};