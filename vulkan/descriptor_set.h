#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_utils.h"
#include "descriptor_set_layout.h"
#include "descriptor_pool.h"
#include "video_buffer.h"
#include "texture.h"


class DescriptorSet {
public:
    VkDescriptorSet descriptor_set;

    DescriptorSet() = default;
    void create(VkDevice& device, DescriptorSetLayout& layout, DescriptorPool& pool);
    void write_uniform_buffer(uint32_t binding, VideoBuffer& buffer);
    void write_combined_image_sampler(uint32_t binding, Texture& texture);
    void write_storage_buffer(uint32_t binding, VideoBuffer& buffer);
};