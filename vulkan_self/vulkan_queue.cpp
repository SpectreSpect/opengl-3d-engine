#include "vulkan_queue.h"

VulkanQueue::VulkanQueue(VkDevice device, QueueLocation location, VulkanQueueType type)
    : m_location(location), m_type(type)
{
    LOG_METHOD();

    logger.check(device != VK_NULL_HANDLE, "Device has not been initialized");

    vkGetDeviceQueue(device, location.family_index, location.queue_index, &m_queue);
}

VulkanQueue::VulkanQueue(VulkanQueue&& other) noexcept
    :   m_queue(std::exchange(other.m_queue, VK_NULL_HANDLE)) ,
        m_location(std::exchange(other.m_location, QueueLocation{})),
        m_type(std::exchange(other.m_type, VulkanQueueType::Graphics)) {}

VulkanQueue& VulkanQueue::operator=(VulkanQueue&& other) noexcept {
    if (this != &other) {
        m_queue = std::exchange(other.m_queue, VK_NULL_HANDLE);
        m_location = std::exchange(other.m_location, QueueLocation{});
        m_type = std::exchange(other.m_type, VulkanQueueType::Graphics);
    }

    return *this;
}

VkQueue VulkanQueue::handle() const noexcept {
    return m_queue;
}

QueueLocation VulkanQueue::location() const noexcept {
    return m_location;
}

VulkanQueueType VulkanQueue::type() const noexcept {
    return m_type;
}

QueueFamilyInfo VulkanQueue::family_info() const noexcept {
    return QueueFamilyInfo{
        .queue_family_index = m_location.family_index,
        .queue_family_type = m_type
    };
}
