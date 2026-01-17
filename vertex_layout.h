#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vertex_attribute.h"

#include "vector"


class VertexLayout {
public:
    void add(const VertexAttribute& attr);

    void apply() const;

private:
    std::vector<VertexAttribute> attributes;
};
