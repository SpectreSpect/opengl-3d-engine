#pragma once
#include "../vulkan_engine.h"
#include "command_pool.h"
#include "command_buffer.h"
#include "image/texture2d.h"

class ResourceLoader {
public:
    VulkanEngine* engine = nullptr;
    CommandPool command_pool;
    CommandBuffer command_buffer;
    VideoBuffer staging_buffer;
    Fence fence;

    ResourceLoader() = default;
    void create(VulkanEngine& engine, VkDeviceSize staging_buffer_size);

    Texture2D load_hdr_texture2d(const std::string& filepath, uint32_t mip_levels, VkImageUsageFlags usage);
};