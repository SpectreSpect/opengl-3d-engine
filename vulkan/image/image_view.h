#pragma once
#include "image_resource.h"
#include <stdexcept>
#include "../vulkan_utils.h"

class ImageView {
public:
    struct CreateDesc {
        ImageResource* image = nullptr;
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageSubresourceRange subresource_range{};
        VkComponentMapping components{};
        VkImageViewCreateFlags flags = 0;
    };

    ImageResource* image = nullptr;
    VkImageView view = VK_NULL_HANDLE;

    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange subresource_range{};
    VkComponentMapping components{};
    VkImageViewCreateFlags flags = 0;

    ImageView() = default;

    static CreateDesc texture2d_desc(ImageResource* image);
    static CreateDesc cubemap_desc(ImageResource* image);
    static CreateDesc cubemap_mip_desc(ImageResource* image, uint32_t mip_level);

    void create(VulkanEngine& engine, const CreateDesc& desc);
    void destroy(VkDevice device);
};