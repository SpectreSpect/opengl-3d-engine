#pragma once
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "drawable.h"
#include "transformable.h"


class Mesh : public Drawable, public Transformable {
public:
    VAO* vao;
    VBO* vbo;
    EBO* ebo;
    VertexLayout* vertex_layout;
    
    
    Mesh(std::vector<float> vertices, std::vector<unsigned int> indices, VertexLayout* vertex_layout);
    // Mesh(float* vertices, unsigned int* indices, int vertices_size, int indices_size, Program* shader, VertexLayout* vertex_layout);
    void draw(RenderState state) override;
};