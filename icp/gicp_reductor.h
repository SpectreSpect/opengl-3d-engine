#pragma once

#include <utility>
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

class GICPReductor {
public:
    struct GICPReductorUniform {
        uint32_t input_count;
    };

    struct GICPPartial {
        double H[6][6];
        double g[6];
        double total_weighted_sq_error;
        uint32_t valid_count;
    };
    static_assert(sizeof(GICPReductor::GICPPartial) == 352);
    

    GICPReductor() = default;
    GICPReductor(VulkanEngine& engine);

    GICPReductor(const GICPReductor&) = delete;
    GICPReductor& operator=(const GICPReductor&) = delete;

    GICPReductor(GICPReductor&& other) noexcept;
    GICPReductor& operator=(GICPReductor&& other) noexcept;

    uint32_t reduce_step(VideoBuffer& partial_src, VideoBuffer& partial_dst, uint32_t input_count);
    GICPPartial reduce(VideoBuffer& partial_src, VideoBuffer& partial_dst, uint32_t input_count);

private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule shader_module;
    
    uint32_t compute_queue_family_id = 0;
    VkQueue compute_queue = VK_NULL_HANDLE;

    CommandPool command_pool;
    CommandBuffer command_buffer;
};