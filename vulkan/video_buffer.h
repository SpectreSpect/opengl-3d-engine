#pragma once
#include <vulkan/vulkan.h>
#include <cstring>
#include "vulkan_utils.h"

class VulkanEngine;

class VideoBuffer {
public:
    VkBuffer buffer;
    VkDeviceMemory buffer_memory;
    VkDevice* device = nullptr;

    VideoBuffer() = default;
    VideoBuffer(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size, 
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS, 
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);
    VideoBuffer(VulkanEngine& engine, VkDeviceSize size, 
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS, 
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);
    
    void create(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size, 
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS, 
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);
    void create(VulkanEngine& engine, VkDeviceSize size, 
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS, 
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);
    
    void update_data(void* data, VkDeviceSize size, VkDeviceSize offset = 0, VkMemoryMapFlags flags = 0);
};