#pragma once
#include <vulkan/vulkan.h>
#include <iostream>

namespace vulkan_utils {
    
    inline VkBufferUsageFlags VK_BUFFER_USAGE_UNIVERSAL_BITS = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    inline VkMemoryPropertyFlags VK_MEMORY_PROPERTY_DEFAULT = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    inline void vk_check(VkResult res, const char* what) {
        if (res != VK_SUCCESS) {
            throw std::runtime_error(std::string(what) + " failed with VkResult = " + std::to_string(res));
        }
    }

    inline uint32_t find_memory_type(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type");
    }

}

