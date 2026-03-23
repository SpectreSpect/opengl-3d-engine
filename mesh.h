#pragma once
#include "vulkan_engine.h"
#include "drawable.h"
#include "transformable.h"
#include "vulkan/video_buffer.h"
#include "vulkan/vulkan_vertex_layout.h"

#include "pbr_uniform.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>


// #include "vao.h"
// #include "vbo.h"
// #include "ebo.h"
// #include "vertex_layout.h"
// #include "program.h"




class Mesh : public Drawable, public Transformable {
public:
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };


    VideoBuffer vertex_buffer;
    VideoBuffer index_buffer;
    uint32_t num_indices = 0;
    
    Mesh() = default;
    Mesh(VulkanEngine& engine, void* vertex_data, uint32_t vertex_data_size_bytes, unsigned int* index_data, uint32_t index_data_size_bytes);
    // Mesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, VulkanVertexLayout& vertex_layout);
    // Mesh(const SSBO& vertex_buffer, const SSBO& index_buffer, VertexLayout* vertex_layout);
    // ~Mesh();
    // Mesh(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, VertexLayout* vertex_layout);
    
    // void update(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, GLenum usage = GL_DYNAMIC_DRAW);
    // void update(const void* vertex_data, size_t vertex_data_size, const void* index_data, size_t index_data_size, GLenum usage = GL_DYNAMIC_DRAW);
    
    // Mesh(float* vertices, unsigned int* indices, int vertices_size, int indices_size, Program* shader, VertexLayout* vertex_layout);
    void draw(RenderState state) override;
};