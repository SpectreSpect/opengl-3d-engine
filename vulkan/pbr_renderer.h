#pragma once
#include "renderer.h"
#include "graphics_pipeline.h"
#include "shader_module.h"
#include "image/cubemap.h"

class PBRRenderer : public Renderer {
public:
    struct PBRUniform {
        glm::mat4 mvp;
    };
    
    VulkanEngine* engine;
    
    ShaderModule* vertex_shader;
    ShaderModule* fragment_shader;

    GraphicsPipeline pipeline;

    VideoBuffer uniform_buffer;
    // Texture2D albedo;
    // Cubemap env_map;
    DescriptorSetBundle descriptor_set_bundle;

    PBRRenderer() = default;
    PBRRenderer(VulkanEngine& engine, ShaderModule& vertex_shader, ShaderModule& fragment_shader);
    // PBRRenderer(VulkanEngine& engine, GraphicsPipeline& pipeline);

    virtual void render(Drawable& drawable, Camera& camera) override;
    virtual void render_mesh(Mesh& mesh, RenderState state) override;
};