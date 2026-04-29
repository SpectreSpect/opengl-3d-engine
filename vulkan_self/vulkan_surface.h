#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanSurface {
public:
    _XCLASS_NAME(VulkanSurface);

    explicit VulkanSurface();
    ~VulkanSurface();
    void destroy();

    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;

    VulkanSurface(VulkanSurface&& other) noexcept;
    VulkanSurface& operator=(VulkanSurface&& other) noexcept;

    VkSurfaceKHR handle() const;

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;    
};