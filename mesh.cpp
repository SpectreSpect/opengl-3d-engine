#include "mesh.h"

Mesh::Mesh(VulkanEngine& engine, void* vertex_data, uint32_t vertex_data_size_bytes, 
           unsigned int* index_data, uint32_t index_data_size_bytes) {
    vertex_buffer = VideoBuffer(engine, vertex_data_size_bytes);
    vertex_buffer.update_data(vertex_data, vertex_data_size_bytes);

    index_buffer =  VideoBuffer(engine, index_data_size_bytes);
    index_buffer.update_data(index_data, index_data_size_bytes);

    num_indices = index_data_size_bytes / sizeof(unsigned int);
}

// Mesh::Mesh(const SSBO& vertex_buffer, const SSBO& index_buffer, VertexLayout* vertex_layout) {
//     vao = new VAO();
    
//     // vbo = new VBO(vertex_buffer.id_, vertex_buffer.size_bytes());
//     // ebo = new EBO(index_buffer.id_, index_buffer.size_bytes());
//     vbo = new VBO(vertex_buffer);
//     ebo = new EBO(index_buffer);

//     vao->init_vao();

//     this->vertex_layout = vertex_layout;

//     vao->setup(*vbo, *ebo, *vertex_layout);
// }

// Mesh::~Mesh() {
//     delete vao;
//     delete vbo;
//     delete ebo;
// }

// void Mesh::update(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, GLenum usage) {
//     if (vbo)
//         vbo->update_mapped(vertices.data(), vertices.size() * sizeof(float), usage);
    
//     if (ebo)
//         ebo->update_mapped(indices.data(), indices.size() * sizeof(unsigned int), usage);
// }

// void Mesh::update(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, GLenum usage) {
//     if (vbo)
//         vbo->update_mapped(vertex_data, vertex_data_size, usage);
    
//     if (ebo)
//         ebo->update_mapped(index_data, index_data_size, usage);
// }

void Mesh::draw(RenderState state) {
    // PBRUniform pbr_uniform = {};

    // glm::mat4 model = get_model_matrix();
    // glm::mat4 world = state.transform * model;
    // glm::mat4 mvp = state.vp * world;

    // pbr_uniform.model = world;
    // pbr_uniform.view = glm::lookAt(
    //                 glm::vec3(2.0f, 2.0f, 2.0f),
    //                 glm::vec3(0.0f, 0.0f, 0.0f),
    //                 glm::vec3(0.0f, 1.0f, 0.0f)
    //             );

    // pbr_uniform.proj = glm::perspective(glm::radians(45.0f),
    //                             float(state.engine->swapchainExtent.width) / float(state.engine->swapchainExtent.height),
    //                             0.1f, 10.0f);

    

    // // state.engine->lighting_system.bind_buffers();
    // // prog->set_uint("xTiles", state.engine->lighting_system.num_clusters.x);
    // // prog->set_uint("yTiles", state.engine->lighting_system.num_clusters.y);
    // // prog->set_uint("zSlices", state.engine->lighting_system.num_clusters.z);
    // // prog->set_float("screenWidth", state.viewport_px.x);
    // // prog->set_float("screenHeight", state.viewport_px.y);
    // // prog->set_uint("max_lights_per_cluster", state.engine->lighting_system.max_lights_per_cluster);
    // // if (state.camera) {
    // //     prog->set_vec3("uViewPos", state.camera->position);
    // //     prog->set_mat4("uView", state.camera->get_view_matrix());
    // //     prog->set_float("nearPlane", state.camera->near);
    // //     prog->set_float("farPlane", state.camera->far);
    // // }

    // state.graphics_pipeline->uniform_buffer.update_data(&pbr_uniform, sizeof(pbr_uniform));

    // state.engine->bind_pipeline(*state.graphics_pipeline);
    // state.engine->bind_vertex_buffer(vertex_buffer);
    // state.engine->bind_index_buffer(index_buffer);
    // state.engine->draw_indexed(num_indices);


    PBRUniform ubo{};

    // glm::mat4 model = get_model_matrix();
    // glm::mat4 world = state.transform * model;
    // glm::mat4 mvp = state.vp * world;
        // float t = static_cast<float>(glfwGetTime());

    float t = 0;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(std::sin(t), 0.0f, 0.0f));
    model = glm::rotate(model, t, glm::vec3(0.0f, 1.0f, 0.0f));


    // prog->set_vec3("uViewPos", state.camera->position);
    // prog->set_mat4("uView", state.camera->get_view_matrix());   
    
    // ubo.mvp = mvp;
    ubo.model = model;
    // // ubo.view_pos = glm::vec4(state.camera->position, 1.0f);
    // ubo.view = state.camera->get_view_matrix();
    // ubo.proj = state.proj;

    

    // ubo.view = glm::lookAt(
    //                 glm::vec3(2.0f, 2.0f, 2.0f),
    //                 glm::vec3(0.0f, 0.0f, 0.0f),
    //                 glm::vec3(0.0f, 1.0f, 0.0f)
    //             );
    ubo.view = state.camera->get_view_matrix();
    ubo.proj = state.camera->get_projection_matrix(1280.0f / 720.0f);
    // ubo.proj  = glm::perspective(glm::radians(45.0f),
    //                             float(state.engine->swapchainExtent.width) / float(state.engine->swapchainExtent.height),
    //                             0.1f, 10.0f);
    // ubo.proj[1][1] *= -1.0f;

    state.graphics_pipeline->uniform_buffer.update_data(&ubo, sizeof(ubo));

    
    state.engine->bind_pipeline(*state.graphics_pipeline);
    state.engine->bind_vertex_buffer(vertex_buffer);
    state.engine->bind_index_buffer(index_buffer);

    state.engine->draw_indexed(num_indices);
}
