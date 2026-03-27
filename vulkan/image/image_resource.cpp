#include "image_resource.h"
#include "../command_buffer.h"
#include "../../vulkan_engine.h"

ImageResource::ImageCreateDesc ImageResource::texture2d_desc(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage) {
    ImageCreateDesc desc{};
    desc.image_type = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.extent = { width, height, 1 };
    desc.mip_levels = 1;
    desc.array_layers = 1;
    desc.usage = usage;
    desc.flags = 0;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    desc.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    return desc;
}

ImageResource::ImageCreateDesc ImageResource::cubemap_desc(VkFormat format, uint32_t face_size, VkImageUsageFlags usage) {
    ImageCreateDesc desc{};
    desc.image_type = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.extent = { face_size, face_size, 1 };
    desc.mip_levels = 1;
    desc.array_layers = 6;
    desc.usage = usage;
    desc.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    desc.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    return desc;
}


void ImageResource::create(VulkanEngine& engine, const ImageCreateDesc& desc) {
    image_type = desc.image_type;
    format = desc.format;
    extent = desc.extent;
    mip_levels = desc.mip_levels;
    array_layers = desc.array_layers;
    usage = desc.usage;
    flags = desc.flags;
    samples = desc.samples;
    tiling = desc.tiling;
    aspect = desc.aspect;
    current_layout = desc.initial_layout;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = desc.flags;
    image_info.imageType = desc.image_type;
    image_info.format = desc.format;
    image_info.extent = desc.extent;
    image_info.mipLevels = desc.mip_levels;
    image_info.arrayLayers = desc.array_layers;
    image_info.samples = desc.samples;
    image_info.tiling = desc.tiling;
    image_info.usage = desc.usage;
    image_info.sharingMode = desc.sharing_mode;
    image_info.initialLayout = desc.initial_layout;

    if (desc.sharing_mode == VK_SHARING_MODE_CONCURRENT) {
        image_info.queueFamilyIndexCount =
            static_cast<uint32_t>(desc.queue_family_indices.size());
        image_info.pQueueFamilyIndices = desc.queue_family_indices.data();
    }

    vulkan_utils::vk_check(
        vkCreateImage(engine.device, &image_info, nullptr, &image),
        "vkCreateImage(ImageResource)"
    );

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(engine.device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = vulkan_utils::find_memory_type(
        engine.physicalDevice,
        mem_requirements.memoryTypeBits,
        desc.memory_properties
    );

    vulkan_utils::vk_check(
        vkAllocateMemory(engine.device, &alloc_info, nullptr, &memory),
        "vkAllocateMemory(ImageResource)"
    );

    vulkan_utils::vk_check(
        vkBindImageMemory(engine.device, image, memory, 0),
        "vkBindImageMemory(ImageResource)"
    );
}

void ImageResource::transition_layout(CommandBuffer& command_buffer, VkImageLayout new_layout, VkPipelineStageFlags src_stage, 
                                      VkPipelineStageFlags dst_stage, VkAccessFlags src_access, VkAccessFlags dst_access) {
    if (command_buffer.buffer == VK_NULL_HANDLE)
        throw std::runtime_error("ImageResource::transition_layout: cmd was VK_NULL_HANDLE");

    if (image == VK_NULL_HANDLE)
        throw std::runtime_error("ImageResource::transition_layout: image was VK_NULL_HANDLE");

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = current_layout;
    barrier.newLayout = new_layout;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = array_layers;

    vkCmdPipelineBarrier(
        command_buffer.buffer,
        src_stage,
        dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    current_layout = new_layout;
}

void ImageResource::transition_undefined_to_compute_write(CommandBuffer& command_buffer) {
    transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );
}

void ImageResource::transition_compute_write_to_fragment_read(CommandBuffer& command_buffer) {
    transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
}