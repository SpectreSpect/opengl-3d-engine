#pragma once
#include "image_resource.h"
#include "image_view.h"
#include "sampler.h"
#include "../video_buffer.h"

class CommandBuffer;

class Cubemap {
public:
    VulkanEngine* engine = nullptr;
    ImageResource image_resource;
    ImageView image_view;
    Sampler sampler;
    uint32_t face_size = 0;

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;


    Cubemap() = default;
    void create(VulkanEngine& engine, uint32_t face_size, VkImageUsageFlags usage, bool srgb = false);
    // void upload_data(const void* data, size_t size_bytes, VkCommandBuffer cmd);
    // void upload_data(const void* data, size_t size_bytes, CommandBuffer& command_buffer, VideoBuffer& staging_buffer);
};