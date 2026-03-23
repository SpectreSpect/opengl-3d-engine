#pragma once
#include <glm/glm.hpp>
#include "camera.h"
#include "vulkan/shader_module.h"
// #include "renderer.h"

class Renderer;

class VulkanEngine;

struct RenderState {
    glm::mat4 proj;
    glm::mat4 vp;       // projection * view
    glm::mat4 transform;    // accumulated parent->world transform
    ShaderModule* vertex_shader;
    ShaderModule* fragment_shader;
    Camera* camera;
    VulkanEngine* engine;
    Renderer* renderer;

    glm::ivec2 viewport_px;
};
