#pragma once

#include "../vulkan_engine.h"
#include "../vulkan/compute_pipeline.h"
#include "../vulkan/fence.h"
#include "../vulkan/image/texture2d.h"
#include "../vulkan/image/cubemap.h"
#include "../vulkan/graphics_pipeline.h"
#include "../vulkan/descriptor_set_bundle.h"
#include "../vulkan/render_target_2d.h"
#include "../vulkan/command_pool.h"
#include "../vulkan/command_buffer.h"


class GICPPass {
public:
    struct GICPPassUniform {
        uint32_t num_points;
    };

    GICPPass() = default;
    void create(VulkanEngine& engine);
    void step();
private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule generate_brdf_lut_cs;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;

    CommandPool command_pool;
    CommandBuffer command_buffer;
};