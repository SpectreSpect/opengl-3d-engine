#pragma once
#include "../vulkan/renderer.h"
#include "../vulkan/graphics_pipeline.h"
#include "../vulkan/shader_module.h"
#include "../vulkan/image/cubemap.h"
#include "../vulkan/lighting_system/lighting_system.h"
#include "../transformable.h"
#include "point_instance.h"

class PointCloud : public Transformable {
public:
    // struct alignas(16) PointInstance {
    //     glm::vec4 pos;
    //     glm::vec4 color;
    // };

    // struct PointCloudUniform {
    //     glm::mat4 model;
    //     glm::mat4 view;
    //     glm::mat4 proj;

    //     glm::vec2 viewport;
    //     float point_size_px;
    //     float point_size_world;
    //     int screen_space_size;
    // };

    VulkanEngine* engine;
    // ShaderModule vertex_shader;
    // ShaderModule fragment_shader;
    // GraphicsPipeline pipeline;
    VideoBuffer instance_buffer;
    uint32_t num_instances = 0;
    // PointCloudPushConstants pc{};
    // pc.model = model;
    uint32_t point_size_px = 2;
    uint32_t point_size_world = 2;
    uint32_t screen_space_size = 1;
    // DescriptorSetBundle descriptor_set_bundle;

    PointCloud() = default;
    void create(VulkanEngine& engine);
    void set_points(const std::vector<PointInstance>& point_instances);
};