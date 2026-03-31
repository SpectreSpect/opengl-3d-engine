#include "video_buffer.h"
#include "../vulkan_engine.h"

#include <vector>

VideoBuffer::VideoBuffer(
    VkDevice& device,
    VkPhysicalDevice& physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
) {
    create(device, physical_device, size, usage, properties);
}

VideoBuffer::VideoBuffer(
    VulkanEngine& engine,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
) {
    create(engine.device, engine.physicalDevice, size, usage, properties);
}

VideoBuffer::~VideoBuffer() {
    destroy();
}

VideoBuffer::VideoBuffer(VideoBuffer&& other) noexcept {
    buffer = other.buffer;
    buffer_memory = other.buffer_memory;
    device = other.device;
    physical_device = other.physical_device;
    size_bytes = other.size_bytes;
    capacity_bytes = other.capacity_bytes;
    usage_flags = other.usage_flags;
    memory_properties = other.memory_properties;

    other.buffer = VK_NULL_HANDLE;
    other.buffer_memory = VK_NULL_HANDLE;
    other.device = nullptr;
    other.physical_device = nullptr;
    other.size_bytes = 0;
    other.capacity_bytes = 0;
    other.usage_flags = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS;
    other.memory_properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT;
}

VideoBuffer& VideoBuffer::operator=(VideoBuffer&& other) noexcept {
    if (this != &other) {
        destroy();

        buffer = other.buffer;
        buffer_memory = other.buffer_memory;
        device = other.device;
        physical_device = other.physical_device;
        size_bytes = other.size_bytes;
        capacity_bytes = other.capacity_bytes;
        usage_flags = other.usage_flags;
        memory_properties = other.memory_properties;

        other.buffer = VK_NULL_HANDLE;
        other.buffer_memory = VK_NULL_HANDLE;
        other.device = nullptr;
        other.physical_device = nullptr;
        other.size_bytes = 0;
        other.capacity_bytes = 0;
        other.usage_flags = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS;
        other.memory_properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT;
    }
    return *this;
}

void VideoBuffer::require_initialized(const char* func) const {
    if (!device || !physical_device || buffer == VK_NULL_HANDLE || buffer_memory == VK_NULL_HANDLE) {
        throw std::runtime_error(std::string(func) + ": buffer is not initialized");
    }
}

void VideoBuffer::require_host_visible(const char* func) const {
    if ((memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
        throw std::runtime_error(
            std::string(func) +
            ": buffer memory is not HOST_VISIBLE; use a staging upload/download path instead"
        );
    }
}

void VideoBuffer::flush_mapped_range(VkDeviceSize offset, VkDeviceSize size) const {
    if ((memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
        return;
    }

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = buffer_memory;
    range.offset = offset;
    range.size = size;

    vulkan_utils::vk_check(
        vkFlushMappedMemoryRanges(*device, 1, &range),
        "vkFlushMappedMemoryRanges"
    );
}

void VideoBuffer::invalidate_mapped_range(VkDeviceSize offset, VkDeviceSize size) const {
    if ((memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
        return;
    }

    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = buffer_memory;
    range.offset = offset;
    range.size = size;

    vulkan_utils::vk_check(
        vkInvalidateMappedMemoryRanges(*device, 1, &range),
        "vkInvalidateMappedMemoryRanges"
    );
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
    physical_device = nullptr;
    size_bytes = 0;
    capacity_bytes = 0;
    usage_flags = vulkan_utils::VK_BUFFER_USAGE_UNIVERSAL_BITS;
    memory_properties = vulkan_utils::VK_MEMORY_PROPERTY_DEFAULT;
}

void VideoBuffer::create(
    VkDevice& device,
    VkPhysicalDevice& physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
) {
    if (size == 0) {
        throw std::invalid_argument("VideoBuffer::create: size == 0");
    }

    destroy();

    this->device = &device;
    this->physical_device = &physical_device;
    this->size_bytes = size;
    this->capacity_bytes = size;
    this->usage_flags = usage;
    this->memory_properties = properties;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vulkan_utils::vk_check(
        vkCreateBuffer(device, &buffer_info, nullptr, &buffer),
        "vkCreateBuffer"
    );

    VkMemoryRequirements mem_requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = vulkan_utils::find_memory_type(
        physical_device,
        mem_requirements.memoryTypeBits,
        properties
    );

    vulkan_utils::vk_check(
        vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory),
        "vkAllocateMemory"
    );

    vulkan_utils::vk_check(
        vkBindBufferMemory(device, buffer, buffer_memory, 0),
        "vkBindBufferMemory"
    );
}

void VideoBuffer::create(
    VulkanEngine& engine,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties
) {
    create(engine.device, engine.physicalDevice, size, usage, properties);
}

void VideoBuffer::reserve(VkDeviceSize min_capacity, bool preserve_contents) {
    require_initialized("VideoBuffer::reserve");

    if (min_capacity <= capacity_bytes) {
        return;
    }

    VkDeviceSize new_capacity = min_capacity;
    if (capacity_bytes > 0) {
        if (capacity_bytes <= std::numeric_limits<VkDeviceSize>::max() / 2) {
            new_capacity = std::max(min_capacity, capacity_bytes * 2);
        }
    }

    VkDevice* old_device = device;
    VkPhysicalDevice* old_physical_device = physical_device;
    VkBufferUsageFlags old_usage = usage_flags;
    VkMemoryPropertyFlags old_properties = memory_properties;
    VkDeviceSize old_size = size_bytes;

    std::vector<unsigned char> old_contents;
    if (preserve_contents && old_size > 0) {
        require_host_visible("VideoBuffer::reserve");
        old_contents.resize(static_cast<size_t>(old_size));
        read_subdata(0, old_contents.data(), old_size);
    }

    create(*old_device, *old_physical_device, new_capacity, old_usage, old_properties);

    if (preserve_contents && !old_contents.empty()) {
        update_data(old_contents.data(), old_size, 0);
        size_bytes = old_size;
    } else {
        size_bytes = 0;
    }
}

void VideoBuffer::resize(VkDeviceSize new_size, bool preserve_contents) {
    if (new_size == 0) {
        size_bytes = 0;
        return;
    }

    if (!device || !physical_device || buffer == VK_NULL_HANDLE || buffer_memory == VK_NULL_HANDLE) {
        throw std::runtime_error("VideoBuffer::resize: buffer is not initialized");
    }

    if (new_size > capacity_bytes) {
        reserve(new_size, preserve_contents);
    }

    size_bytes = new_size;
}

void VideoBuffer::update_data(
    const void* data,
    VkDeviceSize size,
    VkDeviceSize offset,
    VkMemoryMapFlags flags
) {
    require_initialized("VideoBuffer::update_data");

    if (size == 0) {
        if (offset == 0) {
            size_bytes = 0;
        }
        return;
    }

    if (!data) {
        throw std::invalid_argument("VideoBuffer::update_data: data == nullptr while size > 0");
    }

    require_host_visible("VideoBuffer::update_data");

    const VkDeviceSize required_size = offset + size;
    if (required_size > capacity_bytes) {
        reserve(required_size, true);
    }

    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(*device, buffer_memory, offset, size, flags, &mapped_memory),
        "vkMapMemory(update_data)"
    );

    std::memcpy(mapped_memory, data, static_cast<size_t>(size));
    flush_mapped_range(offset, size);
    vkUnmapMemory(*device, buffer_memory);

    if (offset == 0) {
        size_bytes = size;
    } else {
        size_bytes = std::max(size_bytes, required_size);
    }
}

void VideoBuffer::read_subdata(VkDeviceSize offset_bytes, void* out, VkDeviceSize out_bytes) {
    require_initialized("VideoBuffer::read_subdata");

    if (out_bytes == 0) {
        return;
    }

    if (!out) {
        throw std::invalid_argument("VideoBuffer::read_subdata: out == nullptr while out_bytes > 0");
    }

    require_host_visible("VideoBuffer::read_subdata");

    if (offset_bytes + out_bytes > size_bytes) {
        throw std::out_of_range("VideoBuffer::read_subdata: read exceeds logical buffer size");
    }

    void* mapped = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(*device, buffer_memory, offset_bytes, out_bytes, 0, &mapped),
        "vkMapMemory(read_subdata)"
    );

    invalidate_mapped_range(offset_bytes, out_bytes);
    std::memcpy(out, mapped, static_cast<size_t>(out_bytes));
    vkUnmapMemory(*device, buffer_memory);
}

void VideoBuffer::clear_cpu() {
    require_initialized("VideoBuffer::clear_cpu");
    require_host_visible("VideoBuffer::clear_cpu");

    if (capacity_bytes == 0) {
        return;
    }

    void* mapped_memory = nullptr;
    vulkan_utils::vk_check(
        vkMapMemory(*device, buffer_memory, 0, capacity_bytes, 0, &mapped_memory),
        "vkMapMemory(clear_cpu)"
    );

    std::memset(mapped_memory, 0, static_cast<size_t>(capacity_bytes));
    flush_mapped_range(0, capacity_bytes);
    vkUnmapMemory(*device, buffer_memory);
}