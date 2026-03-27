#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../vulkan/cubemap.h"
#include "../vulkan_engine.h"
#include "../vulkan/command_buffer.h"
#include "../vulkan/command_pool.h"
#include "../vulkan/graphics_pipeline.h"

class SkyboxPass {
public:
    struct SkyboxUniform {
        glm::vec4 some_var_temp;
    };

    SkyboxPass() = default;
    void create(VulkanEngine& engine);
    void render(glm::vec3 rotation, Cubemap& environment_map);
private:
    VulkanEngine* engine;
    CommandPool command_pool;
    CommandBuffer command_buffer;
    ShaderModule skybox_vs;
    ShaderModule skybox_fs;

    DescriptorSetBundle descriptor_set_bundle;
    GraphicsPipeline pipeline;
    VideoBuffer uniform_buffer;
};