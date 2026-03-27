#pragma once
#include "image_resource.h"
#include "image_view.h"
#include "sampler.h"
#include "../video_buffer.h"

class CommandBuffer;

class Texture2D {
public:
    VulkanEngine* engine = nullptr;
    ImageResource image_resource;
    ImageView image_view;
    Sampler sampler;
    bool is_srgb = false;

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;


    Texture2D() = default;
    void create(VulkanEngine& engine, uint32_t width, uint32_t height, VkImageUsageFlags usage, bool srgb = false);
    // void upload_data(const void* data, size_t size_bytes, VkCommandBuffer cmd);
    void upload_data(const void* data, size_t size_bytes, CommandBuffer& command_buffer, VideoBuffer& staging_buffer);
};