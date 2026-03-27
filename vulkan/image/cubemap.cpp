#include "cubemap.h"
#include "../command_buffer.h"


void Cubemap::create(VulkanEngine& engine, uint32_t face_size, VkImageUsageFlags usage, bool srgb) {
    if (face_size <= 0)
        throw std::runtime_error("face size must be greater than 0");
    
    this->engine = &engine;
    this->face_size = face_size;

    const VkFormat image_format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    image_resource.create(engine, ImageResource::cubemap_desc(image_format, face_size, usage));
    image_view.create(engine, ImageView::cubemap_desc(&image_resource));
    sampler.create(engine, Sampler::linear_repeat_desc());
}


// void Cubemap::upload_data(const void* data, size_t size_bytes, CommandBuffer& command_buffer, VideoBuffer& staging_buffer) {
//     if (!data)
//         throw std::runtime_error("Cubemap::upload_data: initial_data was nullptr");

//     // 1. Upload pixels into a host-visible staging buffer
//     // staging_buffer.create(
//     //     *engine,
//     //     size_bytes,
//     //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//     //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//     // );

//     staging_buffer.update_data(data, size_bytes);

//     // 2. Prepare image for copy
//     image_resource.transition_layout(
//         command_buffer,
//         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
//         VK_PIPELINE_STAGE_TRANSFER_BIT,
//         0,
//         VK_ACCESS_TRANSFER_WRITE_BIT
//     );

//     // 3. Copy buffer -> image
//     VkBufferImageCopy region{};
//     region.bufferOffset = 0;
//     region.bufferRowLength = 0;   // tightly packed
//     region.bufferImageHeight = 0; // tightly packed

//     region.imageSubresource.aspectMask = image_resource.aspect;
//     region.imageSubresource.mipLevel = 0;
//     region.imageSubresource.baseArrayLayer = 0;
//     region.imageSubresource.layerCount = 1;

//     region.imageOffset = {0, 0, 0};
//     region.imageExtent = image_resource.extent;

//     vkCmdCopyBufferToImage(
//         command_buffer.buffer,
//         staging_buffer.buffer,
//         image_resource.image,
//         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//         1,
//         &region
//     );

//     // 4. Prepare image for shader sampling
//     image_resource.transition_layout(
//         command_buffer,
//         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         VK_PIPELINE_STAGE_TRANSFER_BIT,
//         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//         VK_ACCESS_TRANSFER_WRITE_BIT,
//         VK_ACCESS_SHADER_READ_BIT
//     );
// }