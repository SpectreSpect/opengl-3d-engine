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

    static uint32_t calc_mip_levels(uint32_t face_size);

    void create(
        VulkanEngine& engine,
        uint32_t face_size,
        uint32_t mip_levels,
        VkImageUsageFlags usage,
        VkFormat format
    );

    void generate_mipmaps(CommandBuffer& command_buffer);
};