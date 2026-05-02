#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;
class VulkanSwapchain;

class VulkanImageView {
public:
    _XCLASS_NAME(VulkanImageView);

    VulkanImageView(const VkImageViewCreateInfo& desc, const VulkanDevice& device);
    ~VulkanImageView();
    void destroy();

    VulkanImageView(const VulkanImageView&) = delete;
    VulkanImageView& operator=(const VulkanImageView&) = delete;
    
    VulkanImageView(VulkanImageView&& other) noexcept;
    VulkanImageView& operator=(VulkanImageView&& other) noexcept;

    VkImageView handle() const noexcept;

    static VkImageViewCreateInfo create_swapchain_desc(VkImage image, VkFormat format);
    static std::vector<VulkanImageView> from_swapchain(const VulkanDevice& device, const VulkanSwapchain& swapchain);
private:
    VkImageView m_image_view = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
