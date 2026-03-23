#pragma once
#include "renderer.h"
#include "graphics_pipeline.h"

class PBRRenderer : public Renderer {
public:
    struct PBRUniform {
        glm::mat4 mvp;
    };

    VulkanEngine* engine;
    GraphicsPipeline* pipeline;

    PBRRenderer() = default;
    PBRRenderer(VulkanEngine& engine, GraphicsPipeline& pipeline);

    virtual void render(Drawable& drawable, Camera& camera) override;
    virtual void render_mesh(Mesh& mesh, RenderState state) override;
};