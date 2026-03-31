#pragma once

#include "../vulkan_engine.h"
#include "compute_pipeline.h"
#include "fence.h"
#include "image/texture2d.h"
#include "image/cubemap.h"
#include "graphics_pipeline.h"
#include "descriptor_set_bundle.h"
#include "render_target_2d.h"
#include "command_pool.h"
#include "command_buffer.h"


class ComputeProgram {
public:
    ComputeProgram() = default;
    void create(VulkanEngine& engine, ShaderModule& shader, uint32_t uniform_buffer_size_bytes, DescriptorSetBundle& descriptor_set_bundle);

    void update_uniform(void* data, uint32_t size_bytes);

    void begin();
    void end();
    void end_submit_wait();
    void dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups);

    bool started = false;
    bool ended = true;

    CommandPool command_pool;
    CommandBuffer command_buffer;
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle* descriptor_set_bundle = nullptr;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule* shader;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;
};