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


class GICPPass {
public:
    struct GICPPassUniform {
        glm::vec4 position;
        glm::vec4 rotation;
        uint32_t num_source_points;
        uint32_t num_target_points;
        uint32_t num_hash_table_slots;
    };

    struct OutputBuffer {
        // glm::mat4 model;
        glm::vec4 position;
        glm::vec4 rotation;
    };

    GICPPass() = default;
    void create(VulkanEngine& engine);
    // void step(PointCloud& source_point_cloud, PointCloud& target_point_cloud, VideoBuffer& source_normal_buffer, VideoBuffer& target_normal_buffer);
    void step(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer);
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

    VideoBuffer output_buffer;

    // VideoBuffer map_uniform_buffer;
    // VideoBuffer map_hash_table_buffer;
    // VideoBuffer map_point_buffer;
    // VideoBuffer map_point_count_buffer;
};