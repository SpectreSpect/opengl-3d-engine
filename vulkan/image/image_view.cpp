#include "image_view.h"
#include "../../vulkan_engine.h"

ImageView::CreateDesc ImageView::texture2d_desc(ImageResource* image) {
    CreateDesc desc{};
    desc.image = image;
    desc.view_type = VK_IMAGE_VIEW_TYPE_2D;
    desc.format = image->format;
    desc.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    desc.subresource_range.aspectMask = image->aspect;
    desc.subresource_range.baseMipLevel = 0;
    desc.subresource_range.levelCount = image->mip_levels;
    desc.subresource_range.baseArrayLayer = 0;
    desc.subresource_range.layerCount = 1;
    desc.flags = 0;
    return desc;
}

ImageView::CreateDesc ImageView::cubemap_desc(ImageResource* image) {
    CreateDesc desc{};
    desc.image = image;
    desc.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    desc.format = image->format;
    desc.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    desc.subresource_range.aspectMask = image->aspect;
    desc.subresource_range.baseMipLevel = 0;
    desc.subresource_range.levelCount = image->mip_levels;
    desc.subresource_range.baseArrayLayer = 0;
    desc.subresource_range.layerCount = 6;
    desc.flags = 0;
    return desc;
}

void ImageView::create(VulkanEngine& engine, const CreateDesc& desc) {
    if (!desc.image)
        throw std::runtime_error("ImageView::create: desc.image was nullptr");

    if (desc.image->image == VK_NULL_HANDLE)
        throw std::runtime_error("ImageView::create: desc.image->image was VK_NULL_HANDLE");

    image = desc.image;
    view_type = desc.view_type;
    format = desc.format;
    subresource_range = desc.subresource_range;
    components = desc.components;
    flags = desc.flags;

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.flags = desc.flags;
    view_info.image = desc.image->image;
    view_info.viewType = desc.view_type;
    view_info.format = desc.format;
    view_info.components = desc.components;
    view_info.subresourceRange = desc.subresource_range;

    vulkan_utils::vk_check(
        vkCreateImageView(engine.device, &view_info, nullptr, &view),
        "vkCreateImageView(ImageView)"
    );
}

void ImageView::destroy(VkDevice device) {
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    image = nullptr;
    view_type = VK_IMAGE_VIEW_TYPE_2D;
    format = VK_FORMAT_UNDEFINED;
    subresource_range = {};
    components = {};
    flags = 0;
}