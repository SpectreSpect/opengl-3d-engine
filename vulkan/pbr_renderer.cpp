#include "pbr_renderer.h"
#include "../mesh.h"

PBRRenderer::PBRRenderer(VulkanEngine& engine, ShaderModule& vertex_shader, ShaderModule& fragment_shader) {
    this->vertex_shader = &vertex_shader;
    this->fragment_shader = &fragment_shader;
    this->engine = &engine;

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
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_FRAGMENT_BIT); // albedo
    descriptor_set_bundle = builder.create(engine.device);

    pipeline = GraphicsPipeline(engine, descriptor_set_bundle, vertex_layout, vertex_shader, fragment_shader);
}


void PBRRenderer::render(Drawable& drawable, Camera& camera) {
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
    render_state.vp = render_state.proj * camera.get_view_matrix();

    drawable.draw(render_state);
}

void PBRRenderer::render_mesh(Mesh& mesh, RenderState state) {
    PBRUniform ubo{};

    glm::mat4 model = mesh.get_model_matrix();
    glm::mat4 world = state.transform * model;
    glm::mat4 mvp = state.vp * world;

    ubo.mvp = mvp;
    uniform_buffer.update_data(&ubo, sizeof(ubo));
    
    state.engine->bind_pipeline(pipeline);
    state.engine->bind_vertex_buffer(mesh.vertex_buffer);
    state.engine->bind_index_buffer(mesh.index_buffer);

    state.engine->draw_indexed(mesh.num_indices);
}