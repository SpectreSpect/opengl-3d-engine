#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace math_utils {
    inline glm::vec3 bary_coords(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& p) {
        glm::vec3 v0 = b - a;
        glm::vec3 v1 = c - a;
        glm::vec3 v2 = p - a;

        float d00 = glm::dot(v0, v0);
        float d01 = glm::dot(v0, v1);
        float d11 = glm::dot(v1, v1);
        float d20 = glm::dot(v2, v0);
        float d21 = glm::dot(v2, v1);

        float denom = d00 * d11 - d01 * d01;
        if (std::abs(denom) < 1e-8f) return glm::vec3(-1.0f);

        float v = (d11 * d20 - d01 * d21) / denom;
        float w = (d00 * d21 - d01 * d20) / denom;
        float u = 1.0f - v - w;
        return glm::vec3(u, v, w);
    }

    inline int floor_div(int a, int b) {
        int q = a / b;
        int r = a % b;
        if (r < 0) --q;
        return q;
    }

    inline int floor_mod(int a, int b) {
        int r = a % b;
        if (r < 0) r += b;
        return r;
    }
}