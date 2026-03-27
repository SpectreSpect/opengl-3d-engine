#include "texture2d.h"
#include "../command_buffer.h"

#include <stdexcept>
#include <utility>

Texture2D::Texture2D(Texture2D&& other) noexcept {
    *this = std::move(other);
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this == &other)
        return *this;

    engine = other.engine;
    image_resource = other.image_resource;
    image_view = other.image_view;
    sampler = other.sampler;

    other.engine = nullptr;
    other.image_resource.image = VK_NULL_HANDLE;
    other.image_resource.memory = VK_NULL_HANDLE;
    other.image_view.view = VK_NULL_HANDLE;
    other.sampler.sampler = VK_NULL_HANDLE;

    return *this;
}

uint32_t Texture2D::calc_mip_levels(uint32_t width, uint32_t height) {
    return ImageResource::calc_mip_levels(width, height);
}

void Texture2D::create(
    VulkanEngine& engine,
    uint32_t width,
    uint32_t height,
    uint32_t mip_levels,
    VkImageUsageFlags usage,
    VkFormat format
) {
    if (width == 0 || height == 0)
        throw std::runtime_error("Texture2D::create: width and height must be greater than 0");

    if (mip_levels == 0)
        throw std::runtime_error("Texture2D::create: mip_levels must be greater than 0");

    this->engine = &engine;

    VkImageUsageFlags final_usage = usage;

    // We always need transfer dst for upload_data().
    final_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Needed if we want to generate mipmaps with vkCmdBlitImage.
    if (mip_levels > 1)
        final_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    image_resource.create(
        engine,
        ImageResource::texture2d_desc(format, width, height, mip_levels, final_usage)
    );

    image_view.create(engine, ImageView::texture2d_desc(&image_resource));
    Sampler::SamplerCreateDesc sampler_desc = Sampler::linear_repeat_desc();
    sampler_desc.max_lod = float(mip_levels - 1);
    sampler.create(engine, sampler_desc);
}

void Texture2D::generate_mipmaps(CommandBuffer& command_buffer) {
    if (!engine)
        throw std::runtime_error("Texture2D::generate_mipmaps: engine was nullptr");

    image_resource.generate_mipmaps(*engine, command_buffer);
}

void Texture2D::upload_data(
    const void* data,
    size_t size_bytes,
    CommandBuffer& command_buffer,
    VideoBuffer& staging_buffer
) {
    if (!data)
        throw std::runtime_error("Texture2D::upload_data: initial_data was nullptr");

    if (!engine)
        throw std::runtime_error("Texture2D::upload_data: engine was nullptr");

    staging_buffer.update_data(data, size_bytes);

    image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = image_resource.aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = image_resource.extent;

    vkCmdCopyBufferToImage(
        command_buffer.buffer,
        staging_buffer.buffer,
        image_resource.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    if (image_resource.mip_levels > 1) {
        generate_mipmaps(command_buffer);
    } else {
        image_resource.transition_layout(
            command_buffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );
    }
}