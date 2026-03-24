#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "../../vulkan_engine.h"
#include "../texture2d.h"
#include "../cubemap.h"
#include "../graphics_pipeline.h"
#include "../descriptor_set_bundle.h"
#include "../render_target_2d.h"

class Mesh;

class EquirectToCubemapPass {
public:
    struct EquirectToCubemapPassUniform {
        glm::mat4 proj;
        glm::mat4 view;
    };

    EquirectToCubemapPass() = default;

    void create(VulkanEngine& engine, uint32_t size);
    void render(Mesh& cube_mesh, Texture2D& hdr_texture, Cubemap& output_cubemap);
    void destroy();

private:
    VulkanEngine* engine = nullptr;
    bool render_target_initialized = false;

    GraphicsPipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    RenderTarget2D render_target;
    RenderPass render_pass;

    ShaderModule vertex_shader;
    ShaderModule fragment_shader;

    VideoBuffer uniform_buffer;
};