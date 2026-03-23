#include "pbr_renderer.h"
#include "../mesh.h"

PBRRenderer::PBRRenderer(VulkanEngine& engine, GraphicsPipeline& pipeline) {
    this->pipeline = &pipeline;
    this->engine = &engine;
}

void PBRRenderer::render(Drawable& drawable, Camera& camera) {
    if (!engine)
        throw "'engine' was nullptr";
    if (!pipeline)
        throw "'pipeline' was nullptr";

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
    pipeline->uniform_buffer.update_data(&ubo, sizeof(ubo));
    
    state.engine->bind_pipeline(*pipeline);
    state.engine->bind_vertex_buffer(mesh.vertex_buffer);
    state.engine->bind_index_buffer(mesh.index_buffer);

    state.engine->draw_indexed(mesh.num_indices);
}