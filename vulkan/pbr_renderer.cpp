#include "pbr_renderer.h"
#include "../mesh.h"

PBRRenderer::PBRRenderer(VulkanEngine& engine, ShaderModule& vertex_shader, ShaderModule& fragment_shader, float exposure) {
    this->vertex_shader = &vertex_shader;
    this->fragment_shader = &fragment_shader;
    this->engine = &engine;
    this->exposure = exposure;

    VulkanVertexLayout vertex_layout;
    LayoutInitializer layout_initializer = vertex_layout.get_initializer();
    layout_initializer.add(AttrFormat::FLOAT4);
    layout_initializer.add(AttrFormat::FLOAT4);
    layout_initializer.add(AttrFormat::FLOAT4);
    layout_initializer.add(AttrFormat::FLOAT2);
    layout_initializer.add(AttrFormat::FLOAT4);
    VkPipelineVertexInputStateCreateInfo vertexInput = vertex_layout.get_vertex_intput();

    uniform_buffer = VideoBuffer(engine, sizeof(PBRUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
    // builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // env_map
    // builder.add_combined_image_sampler(3, VK_SHADER_STAGE_FRAGMENT_BIT); // irradiance_map
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // irradianceMap
    builder.add_combined_image_sampler(2, VK_SHADER_STAGE_FRAGMENT_BIT); // prefilterMap
    builder.add_combined_image_sampler(3, VK_SHADER_STAGE_FRAGMENT_BIT); // uBrdfLUT
    builder.add_storage_buffer(4, VK_SHADER_STAGE_FRAGMENT_BIT); // LightSources
    builder.add_storage_buffer(5, VK_SHADER_STAGE_FRAGMENT_BIT); // NumLightsInClusters
    builder.add_storage_buffer(6, VK_SHADER_STAGE_FRAGMENT_BIT); // LightsInClusters
    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(engine, descriptor_set_bundle, vertex_layout, vertex_shader, fragment_shader);
}


void PBRRenderer::render(Drawable& drawable, Camera& camera, Cubemap& irradiance_map, Cubemap& prefilter_map, Texture2D& brdf_lut) {
    if (!engine)
        throw "'engine' was nullptr";

    RenderState render_state = {};

    float aspect = float(engine->swapchainExtent.width) / float(engine->swapchainExtent.height);
    render_state.proj = camera.get_projection_matrix(aspect);
    render_state.vp = render_state.proj * camera.get_view_matrix();
    render_state.engine = engine;
    render_state.transform = glm::mat4(1.0f);
    render_state.renderer = this;
    render_state.camera = &camera;
    render_state.view = camera.get_view_matrix();
    render_state.prefilte_map_mip_levels = prefilter_map.image_resource.mip_levels - 1;
    

    descriptor_set_bundle.bind_combined_image_sampler(1, irradiance_map);
    descriptor_set_bundle.bind_combined_image_sampler(2, prefilter_map);
    descriptor_set_bundle.bind_combined_image_sampler(3, brdf_lut);

    drawable.draw(render_state);
}

void PBRRenderer::render_mesh(Mesh& mesh, RenderState state) {
    PBRUniform ubo{};

    glm::mat4 model = mesh.get_model_matrix();
    glm::mat4 world = state.transform * model;
    glm::mat4 mvp = state.vp * world;

    ubo.mvp = mvp;
    ubo.model = model;
    ubo.view = state.view;

    ubo.viewPos = glm::vec4(state.camera->position, 1.0f);
    ubo.environmentMultiplier = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ubo.pbrParams.y = state.prefilte_map_mip_levels;
    ubo.pbrParams.z = exposure;

    uniform_buffer.update_data(&ubo, sizeof(ubo));
    
    state.engine->bind_pipeline(pipeline);
    state.engine->bind_vertex_buffer(mesh.vertex_buffer);
    state.engine->bind_index_buffer(mesh.index_buffer);

    state.engine->draw_indexed(mesh.num_indices);
}