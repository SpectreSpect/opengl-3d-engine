#pragma once
#include <glm/glm.hpp>
#include "camera.h"

struct Program; // forward

struct RenderState {
    glm::mat4 vp;       // projection * view
    glm::mat4 transform;    // accumulated parent->world transform
    Program* program;   // shader program to use (optional)
    Camera* camera;
};
