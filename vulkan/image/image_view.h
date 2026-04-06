#pragma once
#include <utility>
#include <stdexcept>

#include "image_resource.h"
#include "../vulkan_utils.h"

class VulkanEngine;

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

    VulkanEngine* engine = nullptr;
    ImageResource* image = nullptr;
    VkImageView view = VK_NULL_HANDLE;

    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange subresource_range{};
    VkComponentMapping components{};
    VkImageViewCreateFlags flags = 0;

    ImageView() = default;
    ImageView(VulkanEngine* engine, const CreateDesc& desc);

    ~ImageView();
    void destroy();

    ImageView(const ImageView&) = delete;
    ImageView& operator=(const ImageView&) = delete;

    ImageView(ImageView&& other) noexcept;
    ImageView& operator=(ImageView&& other) noexcept;

    static CreateDesc texture2d_desc(ImageResource* image);
    static CreateDesc cubemap_desc(ImageResource* image);
    static CreateDesc cubemap_mip_desc(ImageResource* image, uint32_t mip_level);
};