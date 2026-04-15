#pragma once
#include "../../vulkan/renderer.h"
#include "../../vulkan/graphics_pipeline.h"
#include "../../vulkan/shader_module.h"
#include "../../vulkan/image/cubemap.h"
#include "../../vulkan/lighting_system/lighting_system.h"
#include "../../mesh.h"
#include "../point_cloud.h"
#include "../point_instance.h"
#include "../../camera.h"


class PointCloudGenerator {
public:
    struct PointCloudGeneratorUniform {

        glm::mat4 view;
        glm::mat4 proj;

        glm::vec2 viewport;
    };

    // struct PointCloudGeneratorPushConstants {
    //     glm::vec4 color;
    //     glm::mat4 model;
    //     float point_size_px;
    //     float point_size_world;
    //     int screen_space_size;
    // };
    // uint32_t num_instances = 0;

    PointCloudGenerator() = default;
    void create(VulkanEngine& engine);
    PointCloud generate(glm::vec3 position, glm::vec3 rotation, float time, 
                        glm::vec3 prev_position, glm::vec3 prev_rotation, float prev_time);
    // 
    // void render(PointCloud& point_cloud, Camera& camera);
    CommandPool command_pool;
    CommandBuffer command_buffer;
private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule compute_shader;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;
    VideoBuffer point_instance_buffer;
};