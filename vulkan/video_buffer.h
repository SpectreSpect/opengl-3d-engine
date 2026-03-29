#pragma once
#include <vulkan/vulkan.h>
#include <cstring>
#include "vulkan_utils.h"

class VulkanEngine;

class VideoBuffer {
public:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    VkDevice* device = nullptr;
    VkDeviceSize size_bytes = 0;

    VideoBuffer() = default;
    VideoBuffer(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size,
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);
    VideoBuffer(VulkanEngine& engine, VkDeviceSize size,
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);

    ~VideoBuffer();

    VideoBuffer(const VideoBuffer&) = delete;
    VideoBuffer& operator=(const VideoBuffer&) = delete;

    VideoBuffer(VideoBuffer&& other) noexcept;
    VideoBuffer& operator=(VideoBuffer&& other) noexcept;

    void create(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size,
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);
    void create(VulkanEngine& engine, VkDeviceSize size,
                VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
                VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT);

    void update_data(const void* data, VkDeviceSize size, VkDeviceSize offset = 0, VkMemoryMapFlags flags = 0);
    void clear();

    void read_subdata(VkDeviceSize offset_bytes, void* out, VkDeviceSize out_bytes);
    void destroy();
};