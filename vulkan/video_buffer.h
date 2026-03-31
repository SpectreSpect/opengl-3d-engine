#pragma once

#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include "vulkan_utils.h"

class VulkanEngine;

class VideoBuffer {
public:
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;

    VkDevice* device = nullptr;
    VkPhysicalDevice* physical_device = nullptr;

    // Logical size currently in use.
    VkDeviceSize size_bytes = 0;

    // Allocated capacity.
    VkDeviceSize capacity_bytes = 0;

    // Stored so the buffer can recreate itself on growth.
    VkBufferUsageFlags usage_flags = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS;
    VkMemoryPropertyFlags memory_properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT;

    VideoBuffer() = default;

    VideoBuffer(
        VkDevice& device,
        VkPhysicalDevice& physical_device,
        VkDeviceSize size,
        VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
        VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT
    );

    VideoBuffer(
        VulkanEngine& engine,
        VkDeviceSize size,
        VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
        VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT
    );

    ~VideoBuffer();

    VideoBuffer(const VideoBuffer&) = delete;
    VideoBuffer& operator=(const VideoBuffer&) = delete;

    VideoBuffer(VideoBuffer&& other) noexcept;
    VideoBuffer& operator=(VideoBuffer&& other) noexcept;

    void create(
        VkDevice& device,
        VkPhysicalDevice& physical_device,
        VkDeviceSize size,
        VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
        VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT
    );

    void create(
        VulkanEngine& engine,
        VkDeviceSize size,
        VkBufferUsageFlags usage = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS,
        VkMemoryPropertyFlags properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT
    );

    // Ensures capacity >= min_capacity. If preserve_contents is true,
    // old logical contents [0, size_bytes) are copied into the new allocation.
    void reserve(VkDeviceSize min_capacity, bool preserve_contents = true);

    // Changes the logical size. Grows capacity automatically if needed.
    // If the buffer must grow, preserve_contents controls whether old data survives.
    void resize(VkDeviceSize new_size, bool preserve_contents = true);

    // Writes data at offset. Automatically grows the buffer if needed.
    // If offset == 0, logical size becomes 'size'.
    // If offset > 0, logical size becomes max(old_size, offset + size).
    void update_data(
        const void* data,
        VkDeviceSize size,
        VkDeviceSize offset = 0,
        VkMemoryMapFlags flags = 0
    );

    // Reads from the logical data range.
    void read_subdata(VkDeviceSize offset_bytes, void* out, VkDeviceSize out_bytes);

    // Zeros the whole allocated capacity.
    void clear_cpu();

    void destroy();

private:
    void require_initialized(const char* func) const;
    void require_host_visible(const char* func) const;
    void flush_mapped_range(VkDeviceSize offset, VkDeviceSize size) const;
    void invalidate_mapped_range(VkDeviceSize offset, VkDeviceSize size) const;
};