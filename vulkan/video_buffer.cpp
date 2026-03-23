#include "video_buffer.h"


VideoBuffer::VideoBuffer(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size, 
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    create(device, physical_device, size, usage, properties);
}

VideoBuffer::VideoBuffer(VulkanEngine& engine, VkDeviceSize size, 
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    create(engine.device, engine.physicalDevice, size, usage, properties);
}

void VideoBuffer::create(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size, 
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    this->device = &device;
    buffer = VK_NULL_HANDLE;
    buffer_memory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vulkan_utils::vk_check(
        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer),
        "vkCreateBuffer"
    );

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkan_utils::find_memory_type(
        physical_device,
        memRequirements.memoryTypeBits,
        properties
    );

    vulkan_utils::vk_check(
        vkAllocateMemory(device, &allocInfo, nullptr, &buffer_memory),
        "vkAllocateMemory"
    );

    vulkan_utils::vk_check(
        vkBindBufferMemory(device, buffer, buffer_memory, 0),
        "vkBindBufferMemory"
    );
}

void VideoBuffer::create(VulkanEngine& engine, VkDeviceSize size, 
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    create(engine.device, engine.physicalDevice, usage, properties);
}

void VideoBuffer::update_data(void* data, VkDeviceSize size, VkDeviceSize offset, VkMemoryMapFlags flags) {
    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(*device, buffer_memory, offset, size, flags, &mapped_memory),
        "vkMapMemory(vertex)"
    );
    std::memcpy(mapped_memory, data, size);
    vkUnmapMemory(*device, buffer_memory);
}