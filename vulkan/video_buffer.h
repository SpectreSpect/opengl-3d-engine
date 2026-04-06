#pragma once
#include <utility>
#include <cstring>
#include <vulkan/vulkan.h>

#include "vulkan_utils.h"

class VulkanEngine;
class CommandBuffer;

class VideoBuffer {
public:
    VulkanEngine* engine = nullptr;

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    VkDeviceSize size_bytes = 0;
    VkMemoryPropertyFlags memory_properties = 0;

    VideoBuffer() = default;
    VideoBuffer(
        VulkanEngine* engine,
        VkPhysicalDevice& physical_device, 
        VkDeviceSize size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties
    );

    ~VideoBuffer();

    void destroy();

    VideoBuffer(const VideoBuffer&) = delete;
    VideoBuffer& operator=(const VideoBuffer&) = delete;

    VideoBuffer(VideoBuffer&& other) noexcept;
    VideoBuffer& operator=(VideoBuffer&& other) noexcept;

    void update_data(const void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes = 0, VkMemoryMapFlags flags = 0);
    void clear_cpu();

    void read_subdata(VkDeviceSize offset_bytes, void* out, VkDeviceSize out_bytes);

    void memory_barrier(
        CommandBuffer& command_buffer,
        VkAccessFlags src_access,
        VkAccessFlags dsc_access,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dsc_stage
    );
};