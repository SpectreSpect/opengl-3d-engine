#pragma once
// #include <vulkan/vulkan.h>
// #include <glm/glm.hpp>

#include "../../vulkan_engine.h"
#include "../compute_pipeline.h"
#include "../fence.h"
#include "../texture2d.h"
#include "../cubemap.h"
#include "../graphics_pipeline.h"
#include "../descriptor_set_bundle.h"
#include "../render_target_2d.h"
#include "../command_pool.h"
#include "../command_buffer.h"
// #include "../../math_utils.h"
// #include <utility>

class Mesh;

class EquirectToCubemapPass {
public:
    struct EquirectToCubemapUniform {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t num_layers;
    };

    EquirectToCubemapPass() = default;
    EquirectToCubemapPass(VulkanEngine& engine);
    void create(VulkanEngine& engine);
    void generate(Texture2D& equirectangular_map, Cubemap& output_cubemap);
    void destroy();

private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer equirect_to_cubemap_uniform_buffer;
    ShaderModule equirect_to_cubemap_cs;
    CommandPool command_pool;
    CommandBuffer command_buffer;
    uint32_t cubemap_face_size;

    // bool render_target_initialized = false;

    // GraphicsPipeline pipeline;
    // DescriptorSetBundle descriptor_set_bundle;
    // RenderTarget2D render_target;
    // RenderPass render_pass;

    // ShaderModule vertex_shader;
    // ShaderModule fragment_shader;

    // VideoBuffer uniform_buffer;
};