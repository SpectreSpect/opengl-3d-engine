#include "descriptor_set.h"

void DescriptorSet::create(VkDevice& device, DescriptorSetLayout& layout, DescriptorPool& pool) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool.pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout.layout;

    vulkan_utils::vk_check(
        vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set),
        "vkAllocateDescriptorSets"
    );
}

void DescriptorSet::write_uniform_buffer(uint32_t binding, VideoBuffer& buffer) {
    if (!buffer.device)
        throw std::runtime_error("'device' was nullptr");
    
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer.buffer;
    buffer_info.offset = 0;
    buffer_info.range = buffer.size_bytes;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(*buffer.device, 1, &write, 0, nullptr);
}

void DescriptorSet::write_combined_image_sampler(uint32_t binding, Texture2D& texture) {
    if (!texture.device)
        throw std::runtime_error("'device' was nullptr");

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = texture.view;
    image_info.sampler = texture.sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(*texture.device, 1, &write, 0, nullptr);
}

void DescriptorSet::write_storage_buffer(uint32_t binding, VideoBuffer& buffer) {
    if (!buffer.device)
        throw std::runtime_error("'device' was nullptr");

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer.buffer;
    buffer_info.offset = 0;
    buffer_info.range = buffer.size_bytes;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(*buffer.device, 1, &write, 0, nullptr);
}