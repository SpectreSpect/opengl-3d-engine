#pragma once
#include "../vulkan/renderer.h"
#include "../vulkan/graphics_pipeline.h"
#include "../vulkan/shader_module.h"
#include "../vulkan/image/cubemap.h"
#include "../vulkan/lighting_system/lighting_system.h"
#include "../mesh.h"
#include "point_cloud.h"
#include "point_instance.h"
#include "../camera.h"


class PointCloudPass {
public:
    struct PointQuadVertex {
        glm::vec2 corner;
    };

    struct PointCloudUniform {
        glm::mat4 view;
        glm::mat4 proj;

        glm::vec2 viewport;
    };

    struct PointCloudPushConstants {
        glm::mat4 model;
        float point_size_px;
        float point_size_world;
        int screen_space_size;
    };

    VulkanEngine* engine;
    ShaderModule vertex_shader;
    ShaderModule fragment_shader;
    GraphicsPipeline pipeline;
    VideoBuffer uniform_buffer;
    DescriptorSetBundle descriptor_set_bundle;

    Mesh quad_mesh;

    // uint32_t num_instances = 0;

    PointCloudPass() = default;
    void create(VulkanEngine& engine);
    void render(PointCloud& point_cloud, Camera& camera);
};