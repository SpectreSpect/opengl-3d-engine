#include "video_buffer.h"
#include "../vulkan_engine.h"

VideoBuffer::VideoBuffer(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size,
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    create(device, physical_device, size, usage, properties);
}

VideoBuffer::VideoBuffer(VulkanEngine& engine, VkDeviceSize size,
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    create(engine.device, engine.physicalDevice, size, usage, properties);
}

VideoBuffer::~VideoBuffer() {
    destroy();
}

VideoBuffer::VideoBuffer(VideoBuffer&& other) noexcept {
    buffer = other.buffer;
    buffer_memory = other.buffer_memory;
    device = other.device;
    size_bytes = other.size_bytes;

    other.buffer = VK_NULL_HANDLE;
    other.buffer_memory = VK_NULL_HANDLE;
    other.device = nullptr;
    other.size_bytes = 0;
}

VideoBuffer& VideoBuffer::operator=(VideoBuffer&& other) noexcept {
    if (this != &other) {
        destroy();

        buffer = other.buffer;
        buffer_memory = other.buffer_memory;
        device = other.device;
        size_bytes = other.size_bytes;

        other.buffer = VK_NULL_HANDLE;
        other.buffer_memory = VK_NULL_HANDLE;
        other.device = nullptr;
        other.size_bytes = 0;
    }
    return *this;
}

void VideoBuffer::read_subdata(VkDeviceSize offset_bytes, void* out, VkDeviceSize out_bytes) {
    if (!device)
        throw std::runtime_error("device was null");

    void* mapped = nullptr;
    vkMapMemory(*device, buffer_memory, offset_bytes, out_bytes, 0, &mapped);

    if (!mapped)
        throw std::runtime_error("failed to map");

    std::memcpy(out, mapped, static_cast<size_t>(out_bytes));
    vkUnmapMemory(*device, buffer_memory);
}

void VideoBuffer::destroy() {
    if (device && buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(*device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (device && buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(*device, buffer_memory, nullptr);
        buffer_memory = VK_NULL_HANDLE;
    }
    device = nullptr;
    size_bytes = 0;
}

void VideoBuffer::create(VkDevice& device, VkPhysicalDevice& physical_device, VkDeviceSize size,
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    destroy();

    this->size_bytes = size;
    this->device = &device;

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
    create(engine.device, engine.physicalDevice, size, usage, properties);
}

void VideoBuffer::update_data(const void* data, VkDeviceSize size, VkDeviceSize offset, VkMemoryMapFlags flags) {
    if (!device || buffer_memory == VK_NULL_HANDLE) {
        throw std::runtime_error("VideoBuffer::update_data on uninitialized buffer");
    }

    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(*device, buffer_memory, offset, size, flags, &mapped_memory),
        "vkMapMemory(vertex)"
    );
    std::memcpy(mapped_memory, data, size);
    vkUnmapMemory(*device, buffer_memory);
}

void VideoBuffer::clear_cpu() {
    if (!device || buffer_memory == VK_NULL_HANDLE) {
        throw std::runtime_error("VideoBuffer::clear on uninitialized buffer");
    }

    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(*device, buffer_memory, 0, size_bytes, 0, &mapped_memory),
        "vkMapMemory(clear)"
    );

    std::memset(mapped_memory, 0, static_cast<size_t>(size_bytes));

    // Only needed if memory is not HOST_COHERENT.
    // if (!(memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = buffer_memory;
    range.offset = 0;
    range.size = size_bytes;

    vulkan_utils::vk_check(
        vkFlushMappedMemoryRanges(*device, 1, &range),
        "vkFlushMappedMemoryRanges(clear)"
    );
    // }

    vkUnmapMemory(*device, buffer_memory);
}