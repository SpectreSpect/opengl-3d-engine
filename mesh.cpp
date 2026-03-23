#include "mesh.h"
#include "vulkan/renderer.h"

Mesh::Mesh(VulkanEngine& engine, void* vertex_data, uint32_t vertex_data_size_bytes, 
           unsigned int* index_data, uint32_t index_data_size_bytes) {
    vertex_buffer = VideoBuffer(engine, vertex_data_size_bytes);
    vertex_buffer.update_data(vertex_data, vertex_data_size_bytes);

    index_buffer =  VideoBuffer(engine, index_data_size_bytes);
    index_buffer.update_data(index_data, index_data_size_bytes);

    num_indices = index_data_size_bytes / sizeof(unsigned int);
}


void Mesh::draw(RenderState state) {
    state.renderer->render_mesh(*this, state);
}
