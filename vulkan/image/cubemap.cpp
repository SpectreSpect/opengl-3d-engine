#include "cubemap.h"
#include "../command_buffer.h"

#include <stdexcept>
#include <utility>

Cubemap::Cubemap(Cubemap&& other) noexcept {
    *this = std::move(other);
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept {
    if (this == &other)
        return *this;

    engine = other.engine;
    image_resource = other.image_resource;
    image_view = other.image_view;
    sampler = other.sampler;
    face_size = other.face_size;

    other.engine = nullptr;
    other.face_size = 0;
    other.image_resource.image = VK_NULL_HANDLE;
    other.image_resource.memory = VK_NULL_HANDLE;
    other.image_view.view = VK_NULL_HANDLE;
    other.sampler.sampler = VK_NULL_HANDLE;

    return *this;
}

uint32_t Cubemap::calc_mip_levels(uint32_t face_size) {
    return ImageResource::calc_mip_levels(face_size, face_size);
}

void Cubemap::create(
    VulkanEngine& engine,
    uint32_t face_size,
    uint32_t mip_levels,
    VkImageUsageFlags usage,
    VkFormat format
) {
    if (face_size == 0)
        throw std::runtime_error("Cubemap::create: face size must be greater than 0");

    if (mip_levels == 0)
        throw std::runtime_error("Cubemap::create: mip_levels must be greater than 0");

    this->engine = &engine;
    this->face_size = face_size;

    VkImageUsageFlags final_usage = usage;

    // Useful if you ever upload mip 0 / copy into the cubemap.
    final_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Needed for generic vkCmdBlitImage mip generation.
    if (mip_levels > 1)
        final_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    image_resource.create(
        engine,
        ImageResource::cubemap_desc(format, face_size, mip_levels, final_usage)
    );

    image_view.create(engine, ImageView::cubemap_desc(&image_resource));
    Sampler::SamplerCreateDesc sampler_desc = Sampler::cubemap_desc();
    sampler_desc.max_lod = float(mip_levels - 1);
    sampler.create(engine, sampler_desc);
}

void Cubemap::generate_mipmaps(CommandBuffer& command_buffer) {
    if (!engine)
        throw std::runtime_error("Cubemap::generate_mipmaps: engine was nullptr");

    image_resource.generate_mipmaps(*engine, command_buffer);
}