#include "vulkan_surface.h"

explicit VulkanSurface::VulkanSurface() {

}

VulkanSurface::~VulkanSurface() {

}

void VulkanSurface::destroy() {

}

VulkanSurface::VulkanSurface(VulkanSurface&& other) noexcept {

}

VulkanSurface& VulkanSurface::operator=(VulkanSurface&& other) noexcept {

}

VkSurfaceKHR VulkanSurface::handle() const {
    return m_surface;
}
