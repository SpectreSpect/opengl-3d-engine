#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

struct PBRUniform {
    
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 mvp;
    // glm::vec4 view_pos;

    // mat4 mvp;
    // mat4 model;
    // mat4 view;
    // mat4 proj;
};
