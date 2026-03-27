#include "texture2d.h"
#include "../command_buffer.h"


void Texture2D::create(VulkanEngine& engine, uint32_t width, uint32_t height, VkImageUsageFlags usage, bool srgb) {
    this->engine = &engine;
    this->is_srgb = srgb;

    const VkFormat image_format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    image_resource.create(engine, ImageResource::texture2d_desc(image_format, width, height, usage));
    image_view.create(engine, ImageView::texture2d_desc(&image_resource));
    sampler.create(engine, Sampler::linear_repeat_desc());
}

// bool Texture2D::load_from_file(VulkanEngine& engine, const std::string& filepath, VkImageUsageFlags usage, bool srgb) {
//     stbi_set_flip_vertically_on_load(0);

//     int w = 0;
//     int h = 0;
//     int channels = 0;

//     stbi_uc* pixels = stbi_load(filepath.c_str(), &w, &h, &channels, STBI_rgb_alpha);
//     if (!pixels) {
//         return false;
//     }

//     try {
//         create(engine, w, h, pixels, usage, srgb);
//     } catch (...) {
//         stbi_image_free(pixels);
//         throw;
//     }

//     stbi_image_free(pixels);
//     return true;
// }

// void Texture2D::upload_data(const void* data, size_t size_bytes, VkCommandBuffer cmd) {
//         // 1. Upload pixels into a host-visible staging buffer
//     VideoBuffer staging_buffer;

//     staging_buffer.create(
//         *engine,
//         size_bytes,
//         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//     );

//     upload_data(data, size_bytes, cmd, staging_buffer);
// }

void Texture2D::upload_data(const void* data, size_t size_bytes, CommandBuffer& command_buffer, VideoBuffer& staging_buffer) {
    if (!data)
        throw std::runtime_error("Texture2D::upload_data: initial_data was nullptr");

    // 1. Upload pixels into a host-visible staging buffer
    // staging_buffer.create(
    //     *engine,
    //     size_bytes,
    //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    // );

    staging_buffer.update_data(data, size_bytes);

    // 2. Prepare image for copy
    image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );
    

    // 3. Copy buffer -> image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;   // tightly packed
    region.bufferImageHeight = 0; // tightly packed

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

    // 4. Prepare image for shader sampling
    image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
}