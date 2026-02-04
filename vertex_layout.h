#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vertex_attribute.h"

#include "vector"



class VertexLayout {
public:
    std::vector<VertexAttribute> attributes;

    void add(const VertexAttribute& attr);
    void apply() const;
    size_t find_attribute_id_by_name(std::string attribute_name) const;

    static inline VertexLayout create_default_vertex_layout() {
        VertexLayout vertex_layout;

        vertex_layout.add({
            "position",
            0, 3, GL_FLOAT, GL_FALSE,
            9 * sizeof(float),
            0, 
            0, {0.0f, 0.0f, 0.0f}
        });

        vertex_layout.add({
            "normal",
            1, 3, GL_FLOAT, GL_FALSE,
            9 * sizeof(float),
            3 * sizeof(float),
            0, {0.0f, 1.0f, 0.0f}
        });

        vertex_layout.add({
            "color",
            2, 3, GL_FLOAT, GL_FALSE,
            9 * sizeof(float),
            6 * sizeof(float),
            0, {1.0f, 1.0f, 1.0f}
        });

        return vertex_layout;
    }

    
};
