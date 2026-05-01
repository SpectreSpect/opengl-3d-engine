#pragma once

#include <cstdint>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_queue_types.h"
#include "logger/logger_header.h"

class VulkanQueue {
public:
    _XCLASS_NAME(VulkanQueue);

    explicit VulkanQueue(VkDevice device, QueueLocation location, VulkanQueueType type);

    VulkanQueue(const VulkanQueue&) = delete;
    VulkanQueue& operator=(const VulkanQueue&) = delete;

    VulkanQueue(VulkanQueue&& other) noexcept;
    VulkanQueue& operator=(VulkanQueue&& other) noexcept;

    VkQueue handle() const noexcept;
    QueueLocation location() const noexcept;
    VulkanQueueType type() const noexcept;

private:
    VkQueue m_queue = VK_NULL_HANDLE;
    QueueLocation m_location{};
    VulkanQueueType m_type = VulkanQueueType::Graphics;
};
