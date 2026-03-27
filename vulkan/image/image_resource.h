#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

class VulkanEngine;
class CommandBuffer;

class ImageResource {
public:
    struct ImageCreateDesc {
        VkImageType image_type = VK_IMAGE_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{1, 1, 1};

        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;

        VkImageUsageFlags usage = 0;
        VkImageCreateFlags flags = 0;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        std::vector<uint32_t> queue_family_indices;

        VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkImageType image_type = VK_IMAGE_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent{1, 1, 1};
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    VkImageUsageFlags usage = 0;
    VkImageCreateFlags flags = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    VkImageAspectFlags aspect = 0;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    static ImageCreateDesc texture2d_desc(
        VkFormat format,
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels,
        VkImageUsageFlags usage
    );

    static ImageCreateDesc cubemap_desc(
        VkFormat format,
        uint32_t face_size,
        uint32_t mip_levels,
        VkImageUsageFlags usage
    );

    static uint32_t calc_mip_levels(uint32_t width, uint32_t height);

    void create(VulkanEngine& engine, const ImageCreateDesc& desc);
    void destroy(VkDevice device);

    void transition_layout(
        CommandBuffer& command_buffer,
        VkImageLayout new_layout,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags src_access,
        VkAccessFlags dst_access
    );

    void transition_undefined_to_compute_write(CommandBuffer& command_buffer);
    void transition_compute_write_to_fragment_read(CommandBuffer& command_buffer);

    // Assumes mip 0 already contains valid data.
    // Assumes the image was created with TRANSFER_SRC and TRANSFER_DST usage.
    void generate_mipmaps(VulkanEngine& engine, CommandBuffer& command_buffer);
};