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
};
