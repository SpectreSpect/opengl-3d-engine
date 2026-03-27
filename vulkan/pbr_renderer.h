#pragma once
#include "renderer.h"
#include "graphics_pipeline.h"
#include "shader_module.h"
#include "image/cubemap.h"

class PBRRenderer : public Renderer {
public:
    // struct PBRUniform {
    //     glm::mat4 mvp;
    // };

    struct PBRUniform {
        glm::mat4 mvp;
        glm::mat4 model;
        glm::mat4 view;

        glm::vec4 viewPos;
        glm::vec4 environmentMultiplier;

        glm::uvec4 clusterGrid;   // xTiles, yTiles, zSlices, maxLightsPerCluster
        glm::vec4  screenParams;  // width, height, near, far
        glm::vec4  pbrParams;     // normalStrength, prefilterMaxMip, exposure, unused
    };


    
    VulkanEngine* engine;
    
    ShaderModule* vertex_shader;
    ShaderModule* fragment_shader;

    GraphicsPipeline pipeline;

    VideoBuffer uniform_buffer;
    // Texture2D albedo;
    // Cubemap env_map;
    DescriptorSetBundle descriptor_set_bundle;

    float exposure = 0.3f;

    PBRRenderer() = default;
    PBRRenderer(VulkanEngine& engine, ShaderModule& vertex_shader, ShaderModule& fragment_shader, float exposure = 0.3f);
    // PBRRenderer(VulkanEngine& engine, GraphicsPipeline& pipeline);

    void render(Drawable& drawable, Camera& camera, Cubemap& irradiance_map, Cubemap& prefilter_map, Texture2D& brdf_lut);
    virtual void render_mesh(Mesh& mesh, RenderState state) override;
};