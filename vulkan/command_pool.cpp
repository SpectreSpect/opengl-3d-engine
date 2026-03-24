#include "command_pool.h"

CommandPool::CommandPool(VkDevice& device, VkPhysicalDevice& physical_device) {
    create(device, physical_device);
}

void CommandPool::create(VkDevice& device, VkPhysicalDevice& physical_device) {
    this->device = &device;
    this->physical_device = &physical_device;
    this->compute_queue_family_id = vulkan_utils::find_compute_queue_family(physical_device);

    vkGetDeviceQueue(device, this->compute_queue_family_id, 0, &this->compute_queue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = this->compute_queue_family_id;

    vulkan_utils::vk_check(
        vkCreateCommandPool(device, &poolInfo, nullptr, &pool),
        "vkCreateCommandPool"
    );
}