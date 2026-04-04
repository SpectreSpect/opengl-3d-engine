#include "command_pool.h"
#include "vulkan_utils.h"

CommandPool::CommandPool(VkDevice& device, VkPhysicalDevice& physical_device, uint32_t queue_family_id, VkQueue queue) {
    create(device, physical_device, queue_family_id, queue);
}

CommandPool::CommandPool(CommandPool&& other) noexcept
    : device(std::exchange(other.device, nullptr)),
      physical_device(std::exchange(other.physical_device, nullptr)),
      pool(std::exchange(other.pool, VK_NULL_HANDLE)),
      queue_family_id(std::exchange(other.queue_family_id, 0)),
      queue(std::exchange(other.queue, VK_NULL_HANDLE)) {}

CommandPool& CommandPool::operator=(CommandPool&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    destroy();

    device = std::exchange(other.device, nullptr);
    physical_device = std::exchange(other.physical_device, nullptr);
    pool = std::exchange(other.pool, VK_NULL_HANDLE);
    queue_family_id = std::exchange(other.queue_family_id, 0);
    queue = std::exchange(other.queue, VK_NULL_HANDLE);

    return *this;
}

void CommandPool::create(VkDevice& device, VkPhysicalDevice& physical_device, uint32_t queue_family_id, VkQueue queue) {
    destroy();

    this->device = &device;
    this->physical_device = &physical_device;
    this->queue_family_id = queue_family_id;
    this->queue = queue;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queue_family_id;

    vulkan_utils::vk_check(
        vkCreateCommandPool(device, &poolInfo, nullptr, &pool),
        "vkCreateCommandPool"
    );
}

void CommandPool::destroy() {
    if (device && pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(*device, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }

    device = nullptr;
    physical_device = nullptr;
    queue_family_id = 0;
    queue = VK_NULL_HANDLE;
}