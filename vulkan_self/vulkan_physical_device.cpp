#include "vulkan_physical_device.h"

#include <set>
#include <string>

VulkanPhysicalDevice::VulkanPhysicalDevice(const VulkanInstance& instance, const VulkanSurface& surface)
    :   m_surface(surface.handle())
{
    LOG_METHOD();

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance.handle(), &device_count, nullptr);

    logger.check(device_count > 0, "Failed to find GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance.handle(), &device_count, devices.data());

    for (const auto& device : devices) {
        if (is_device_suitable(device)) {
            m_physical_device = device;
            break;
        }
    }

    logger.check(
        m_physical_device != VK_NULL_HANDLE,
        "Failed to find a suitable GPU"
    );
}

VulkanPhysicalDevice::VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept
    :   m_physical_device(std::exchange(other.m_physical_device, VK_NULL_HANDLE)),
        m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE)) {}

VulkanPhysicalDevice& VulkanPhysicalDevice::operator=(VulkanPhysicalDevice&& other) noexcept {
    if (this != &other) {
        m_physical_device = std::exchange(other.m_physical_device, VK_NULL_HANDLE);
        m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
    }

    return *this;
}

VkPhysicalDevice VulkanPhysicalDevice::handle() const noexcept {
    return m_physical_device;
}

QueueFamilyIndices VulkanPhysicalDevice::find_queue_families(VkPhysicalDevice device) const {
    LOG_METHOD();

    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const auto& queue_family = queue_families[i];

        if (queue_family.queueCount > 0 && (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.graphics_family = i;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

        if (queue_family.queueCount > 0 && present_support) {
            indices.present_family = i;
        }

        if (indices.is_complete()) {
            break;
        }
    }

    return indices;
}

QueueFamilyIndices VulkanPhysicalDevice::find_queue_families() const {
    return find_queue_families(m_physical_device);
}

bool VulkanPhysicalDevice::check_device_extension_support(VkPhysicalDevice device) const {
    LOG_METHOD();

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extension_count,
        available_extensions.data()
    );

    std::set<std::string> required_extensions(
        device_extensions.begin(),
        device_extensions.end()
    );

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

bool VulkanPhysicalDevice::check_device_extension_support() const {
    return check_device_extension_support(m_physical_device);
}

VulkanPhysicalDevice::SwapchainSupportDetails VulkanPhysicalDevice::query_swapchain_support(VkPhysicalDevice device) const {
    LOG_METHOD();

    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device,
        m_surface,
        &details.capabilities
    );

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            m_surface,
            &format_count,
            details.formats.data()
        );
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device,
        m_surface,
        &present_mode_count,
        nullptr
    );

    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            m_surface,
            &present_mode_count,
            details.present_modes.data()
        );
    }

    return details;
}

VulkanPhysicalDevice::SwapchainSupportDetails VulkanPhysicalDevice::query_swapchain_support() const {
    return query_swapchain_support(m_physical_device);
}

bool VulkanPhysicalDevice::is_device_suitable(VkPhysicalDevice device) const {
    LOG_METHOD();

    QueueFamilyIndices indices = find_queue_families(device);
    bool extensions_supported = check_device_extension_support(device);

    bool swapchain_adequate = false;
    if (extensions_supported) {
        SwapchainSupportDetails swapchain_support = query_swapchain_support(device);
        swapchain_adequate =
            !swapchain_support.formats.empty() &&
            !swapchain_support.present_modes.empty();
    }

    return indices.is_complete() && extensions_supported && swapchain_adequate;
}

bool VulkanPhysicalDevice::is_device_suitable() const {
    return is_device_suitable(m_physical_device);
}
