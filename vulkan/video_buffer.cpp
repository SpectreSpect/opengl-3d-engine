#include "video_buffer.h"
#include "../vulkan_engine.h"
#include "command_buffer.h"

VideoBuffer::VideoBuffer(
    VulkanEngine* engine, 
    VkPhysicalDevice& physical_device, 
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties)
    :   engine(engine),
        size_bytes(size_bytes),
        memory_properties(properties)
{
    if (engine == nullptr) {
        std::string message = "VideoBuffer::VideoBuffer: engine was nullptr";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (engine->device == VK_NULL_HANDLE) {
        std::string message = "VideoBuffer::VideoBuffer: engine->device was not initialized";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size_bytes;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vulkan_utils::vk_check(
        vkCreateBuffer(engine->device, &bufferInfo, nullptr, &buffer),
        "vkCreateBuffer"
    );

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(engine->device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vulkan_utils::find_memory_type(
        physical_device,
        memRequirements.memoryTypeBits,
        properties
    );

    vulkan_utils::vk_check(
        vkAllocateMemory(engine->device, &allocInfo, nullptr, &buffer_memory),
        "vkAllocateMemory"
    );

    vulkan_utils::vk_check(
        vkBindBufferMemory(engine->device, buffer, buffer_memory, 0),
        "vkBindBufferMemory"
    );
}

VideoBuffer::~VideoBuffer() {
    destroy();
}

void VideoBuffer::destroy() {
    if (buffer != VK_NULL_HANDLE || buffer_memory != VK_NULL_HANDLE)
    if (engine == nullptr || engine->device == VK_NULL_HANDLE) {
        std::string message = 
            "VideoBuffer::destroy: Memory leak due to inability to destroy object "
            "- missing engine pointer or engine->device not initialized.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(engine->device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }

    if (buffer_memory != VK_NULL_HANDLE) {
        vkFreeMemory(engine->device, buffer_memory, nullptr);
        buffer_memory = VK_NULL_HANDLE;
    }

    engine = nullptr;
}

VideoBuffer::VideoBuffer(VideoBuffer&& other) noexcept
    :   engine(std::exchange(other.engine, nullptr)),
        buffer(std::exchange(other.buffer, VK_NULL_HANDLE)),
        buffer_memory(std::exchange(other.buffer_memory, VK_NULL_HANDLE)),
        size_bytes(std::exchange(other.size_bytes, 0)),
        memory_properties(std::exchange(other.memory_properties, 0))
{

}

VideoBuffer& VideoBuffer::operator=(VideoBuffer&& other) noexcept {
    if (this == &other)
        return *this;
    
    destroy();

    engine = std::exchange(other.engine, nullptr);
    buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
    buffer_memory = std::exchange(other.buffer_memory, VK_NULL_HANDLE);
    size_bytes = std::exchange(other.size_bytes, 0);
    memory_properties = std::exchange(other.memory_properties, 0);

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

void VideoBuffer::memory_barrier(
    CommandBuffer& command_buffer,
    VkAccessFlags src_access,
    VkAccessFlags dsc_access,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dsc_stage)
{
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dsc_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        command_buffer.buffer,
        src_stage,
        dsc_stage,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

void VideoBuffer::update_data(const void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes, VkMemoryMapFlags flags) {
    if (engine == nullptr || engine->device == VK_NULL_HANDLE) {
        std::string message = "VideoBuffer::update_data: engine was nullptr or engine->device was VK_NULL_HANDLE.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (buffer_memory == VK_NULL_HANDLE) {
        std::string message = "VideoBuffer::update_data on uninitialized buffer";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(engine->device, buffer_memory, offset_bytes, size_bytes, flags, &mapped_memory),
        "vkMapMemory(vertex)"
    );

    std::memcpy(mapped_memory, data, size_bytes);

    if (!(memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = buffer_memory;
        range.offset = offset_bytes;
        range.size = size_bytes;

        vulkan_utils::vk_check(
            vkFlushMappedMemoryRanges(engine->device, 1, &range),
            "vkFlushMappedMemoryRanges(update_data)"
        );
    }

    vkUnmapMemory(engine->device, buffer_memory);
}

void VideoBuffer::clear_cpu() {
    if (engine == nullptr || engine->device == VK_NULL_HANDLE) {
        std::string message = "VideoBuffer::clear_cpu: engine was nullptr or engine->device was VK_NULL_HANDLE.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (buffer_memory == VK_NULL_HANDLE) {
        std::string message = "VideoBuffer::clear_cpu on uninitialized buffer";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(engine->device, buffer_memory, 0, size_bytes, 0, &mapped_memory),
        "vkMapMemory(clear)"
    );

    std::memset(mapped_memory, 0, static_cast<size_t>(size_bytes));

    if (!(memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = buffer_memory;
        range.offset = 0;
        range.size = size_bytes;

        vulkan_utils::vk_check(
            vkFlushMappedMemoryRanges(engine->device, 1, &range),
            "vkFlushMappedMemoryRanges(clear)"
        );
    }

    vkUnmapMemory(engine->device, buffer_memory);
}