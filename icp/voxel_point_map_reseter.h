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
#include "../point_cloud/point_cloud.h"
#include "voxel_point_map.h"


class VoxelPointMapReseter {
public:
    struct ReseterUniform {
        uint32_t num_hash_table_slots;
    };

    VoxelPointMapReseter() = default;
    void create(VulkanEngine& engine);
    void reset(VoxelPointMap& voxel_point_map);
private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule shader_module;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;

    CommandPool command_pool;
    CommandBuffer command_buffer;
};