#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct alignas(16) PointInstance {
    glm::vec4 pos;
    glm::vec4 color;
};