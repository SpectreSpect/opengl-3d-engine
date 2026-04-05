#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

// #include "engine3d.h"
// #include "opengl_engine.h"
#include "vulkan_engine.h"
#include <array>
#include <tuple>

// #include "vao.h"
// #include "vbo.h"
// #include "ebo.h"
// #include "vertex_layout.h"
// #include "program.h"
#include "camera.h"
// #include "mesh.h"
// #include "cube.h"
// #include "window.h"
#include "fps_camera_controller.h"
// #include "voxel_engine/chunk.h"
// #include "voxel_engine/voxel_grid.h"
// #include "imgui_layer.h"
// #include "voxel_rastorizator.h"
// #include "ui_elements/triangle_controller.h"
// #include "triangle.h"
// #include "a_star/a_star.h"
// #include "line.h"
// #include "a_star/nonholonomic_a_star.h"
// #include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
// #include "point.h"
// #include "point_cloud/point_cloud_frame.h"
// #include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
// #include "math_utils.h"
// #include "light_source.h"
// #include "circle_cloud.h"
// #include "texture.h"
// #include "cubemap.h"
// #include "skybox.h"
// #include "framebuffer.h"
// #include "sphere.h"
// #include "cube.h"
// #include "quad.h"
// #include "texture_manager.h"
// #include "pbr_skybox.h"
#include <algorithm>
#include <random>
// #include "third_person_camera_controller.h"
// #include "spider.h"
#include "vulkan_window.h"
#include "vulkan/shader_module.h"
#include "vulkan/video_buffer.h"
#include "vulkan/vulkan_vertex_layout.h"
#include "vulkan/graphics_pipeline.h"
#include "vulkan/pbr_renderer.h"
#include "mesh.h"
#include "pbr_uniform.h"
// #include "vulkan/texture2d.h"
#include "vulkan/image/texture2d.h"
#include "vulkan/render_target_2d.h"
#include "vulkan/render_pass.h"
#include "vulkan/image/cubemap.h"
#include "vulkan/command_pool.h"
#include "vulkan/command_buffer.h"
#include "vulkan/compute_pipeline.h"
// #include "vulkan/cubemap.h"
#include "vulkan/image/cubemap.h"
#include "math_utils.h"
#include "vulkan/pbr/equirect_to_cubemap_pass.h"

#include "vulkan/resource_loader.h"
#include "skybox/skybox_pass.h"
#include "vulkan/pbr/brdf_lut_generator.h"
#include "vulkan/pbr/irradiance_map_generator.h"
#include "vulkan/pbr/prefilter_map_generator.h"
#include "vulkan/lighting_system/lighting_system.h"
#include "point_cloud/point_cloud_pass.h"
#include "point_cloud/point_cloud.h"
#include "point_cloud/point_cloud_video.h"
#include "icp/gicp.h"
#include "icp/gicp_pass.h"
#include "icp/voxel_point_map.h"
#include "icp/voxel_map_point_inserter.h"
#include "icp/voxel_point_map_reseter.h"

struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec4 tangent;
};

float clear_col[4] = {0, 0, 0, 1};

struct CubePosVertex {
    glm::vec4 position;
};

struct EquirectToCubemapUniform {
    uint32_t image_width;
    uint32_t image_height;
    uint32_t num_layers;
};

Mesh create_cube_mesh(VulkanEngine& engine, glm::vec4 color) {
    std::vector<Vertex> vertices = {
        // +Z (front)
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Z (back)
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 1.0f) },

        // +X (right)
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f, -1.0f, 1.0f) },

        // -X (left)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4(-1.0f,  0.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 0.0f,  0.0f,  1.0f, 1.0f) },

        // +Y (top)
        { glm::vec4(-0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f,  0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f,  0.5f, -0.5f, 1.0f), glm::vec4( 0.0f,  1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },

        // -Y (bottom)
        { glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f, -0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 0.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4( 0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(1.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
        { glm::vec4(-0.5f, -0.5f,  0.5f, 1.0f), glm::vec4( 0.0f, -1.0f,  0.0f, 0.0f), color, glm::vec2(0.0f, 1.0f), glm::vec4( 1.0f,  0.0f,  0.0f, 1.0f) },
    };

    std::vector<uint32_t> indices = {
        // +Z (front)
        0, 1, 2,
        2, 3, 0,

        // -Z (back)
        4, 5, 6,
        6, 7, 4,

        // +X (right)
        8, 9, 10,
        10, 11, 8,

        // -X (left)
        12, 13, 14,
        14, 15, 12,

        // +Y (top)
        16, 17, 18,
        18, 19, 16,

        // -Y (bottom)
        20, 21, 22,
        22, 23, 20
    };

    Mesh mesh(engine,
              vertices.data(),
              sizeof(Vertex) * vertices.size(),
              indices.data(),
              sizeof(uint32_t) * indices.size());

    return mesh;
}

Mesh create_sphere_mesh(VulkanEngine& engine, glm::vec4 color) {
    constexpr float radius = 0.5f;
    constexpr uint32_t stacks = 32;
    constexpr uint32_t sectors = 64;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve((stacks + 1) * (sectors + 1));
    indices.reserve(stacks * sectors * 6);

    const float pi = 3.14159265358979323846f;
    const float two_pi = 2.0f * pi;

    for (uint32_t i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / static_cast<float>(stacks);
        float phi = v * pi;

        float sin_phi = std::sin(phi);
        float cos_phi = std::cos(phi);

        for (uint32_t j = 0; j <= sectors; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(sectors);
            float theta = u * two_pi;

            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            glm::vec3 normal(
                sin_phi * cos_theta,
                cos_phi,
                sin_phi * sin_theta
            );

            glm::vec3 position = radius * normal;

            glm::vec3 tangent;
            if (sin_phi > 1e-6f) {
                tangent = glm::normalize(glm::vec3(
                    -sin_theta,
                    0.0f,
                    cos_theta
                ));
            } else {
                tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            vertices.push_back({
                glm::vec4(position, 1.0f),
                glm::vec4(glm::normalize(normal), 0.0f),
                color,
                glm::vec2(u, v),
                glm::vec4(tangent, 1.0f)
            });
        }
    }

    for (uint32_t i = 0; i < stacks; ++i) {
        for (uint32_t j = 0; j < sectors; ++j) {
            uint32_t k1 = i * (sectors + 1) + j;
            uint32_t k2 = k1 + sectors + 1;

            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != stacks - 1) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    Mesh mesh(
        engine,
        vertices.data(),
        sizeof(Vertex) * vertices.size(),
        indices.data(),
        sizeof(uint32_t) * indices.size()
    );

    return mesh;
}



// struct alignas(16) HashPoint {
//     glm::vec4 position;
//     glm::vec4 normal;
// };

// struct alignas(16) PointNormal {
//     glm::vec4 normal;
// };

// struct alignas(16) HashTableSlot {
//     glm::uvec2     hash_key;
//     std::uint32_t  _pad0[2];     // forces hash_value to offset 16

//     HashPoint      hash_value;   // offset 16, size 32
//     std::uint32_t  hash_state;   // offset 48
//     std::uint32_t  _pad1[3];     // tail pad so sizeof == 64
// };

// struct alignas(16) Mat3Std430 {
//     glm::vec4 col0; // use xyz, w is padding
//     glm::vec4 col1; // use xyz, w is padding
//     glm::vec4 col2; // use xyz, w is padding
// };

// struct HAndG {
//     double h[6][6];
//     double g[6];
//     uint status; // lock flag
// };


// // struct OutputHG {
// //     double h[6][6];
// //     double g[6];
// // };

// struct alignas(16) Location {
//     glm::vec4 position;
//     glm::vec4 rotation;
// };

// struct alignas(8) TracePoint {
//     uint32_t counter;
//     uint32_t site_id;
//     double value;
//     uint32_t pad0;
// };


// class VoxelMap {
// public:
//     // SSBO map_hash_table_ssbo;
//     // SSBO map_points_ssbo;
//     // SSBO map_normals_ssbo;
//     // SSBO num_map_points_ssbo;

//     // SSBO h_and_g_ssbo;
//     // SSBO source_location_ssbo;

//     // SSBO Mat3Std430_ssbo;

//     // SSBO transform_normal_world_ssbo;
//     // SSBO out_val_ssbo;

//     // SSBO target_ids_ssbo;
//     // SSBO output_hg;
//     // SSBO trace_ssbo;
//     // SSBO num_trace_points_ssbo;
    

//     uint32_t num_map_points = 0;
//     ComputeProgram* add_point_cloud_program;
//     ComputeProgram* align_point_cloud_program;

//     int max_map_points_count = 0;

//     // VoxelMap(ShaderManager& shader_manager, 
//     //          ComputeProgram& add_point_cloud_program, ComputeProgram& align_point_cloud_program, 
//     //          float voxel_size, int hash_table_size, int max_map_points_count) {
//     VoxelMap(float voxel_size, int hash_table_size, int max_map_points_count) {
//         // this->max_map_points_count = max_map_points_count;

//         // map_hash_table_ssbo = SSBO::from_fill(sizeof(std::uint32_t) * 4 + sizeof(HashTableSlot) * hash_table_size, GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // map_points_ssbo = SSBO::from_fill(sizeof(PointInstance) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // map_normals_ssbo = SSBO::from_fill(sizeof(PointNormal) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // target_ids_ssbo = SSBO::from_fill(sizeof(uint32_t) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);\
//         // output_hg = SSBO::from_fill(sizeof(OutputHG) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // trace_ssbo = SSBO::from_fill(sizeof(TracePoint) * (5000), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // num_trace_points_ssbo = SSBO::from_fill(sizeof(uint32_t), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // num_map_points_ssbo = SSBO::from_fill(sizeof(uint32_t), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // h_and_g_ssbo = SSBO::from_fill(sizeof(HAndG), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // source_location_ssbo = SSBO::from_fill(sizeof(Location), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // Mat3Std430_ssbo = SSBO::from_fill(sizeof(Mat3Std430), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // transform_normal_world_ssbo = SSBO::from_fill(sizeof(glm::vec4), GL_DYNAMIC_DRAW, 0u, shader_manager);
//         // out_val_ssbo = SSBO::from_fill(sizeof(glm::dvec4), GL_DYNAMIC_DRAW, 0u, shader_manager);
        
        
//         // this->add_point_cloud_program = &add_point_cloud_program;
//         // this->align_point_cloud_program = &align_point_cloud_program;
//     }

//     void add_point_cloud(Point& source_point_cloud, SSBO& source_normals_ssbo) {
//         // std::uint32_t num_points = point_cloud_video.frames[0].point_cloud.points.size();
//         SSBO source_points_ssbo = SSBO(*source_point_cloud.instance_vbo);
//         std::uint32_t num_source_points = source_point_cloud.instance_count;
//         int x_count = math_utils::div_up_u32(num_source_points, 256);
        
//         map_hash_table_ssbo.bind_base(0);
        
//         map_points_ssbo.bind_base(1);
//         map_normals_ssbo.bind_base(2);
//         num_map_points_ssbo.bind_base(3);
        
//         source_points_ssbo.bind_base(4);
//         source_normals_ssbo.bind_base(5);
        
        
//         // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
//         add_point_cloud_program->set_uint("num_source_points", num_source_points);

//         add_point_cloud_program->use();
//         add_point_cloud_program->dispatch_compute(x_count, 1, 1);
//         glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//         num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

//         // PointNormal normals[5];
//         // map_normals_ssbo.read_subdata(0u, &normals, sizeof(PointNormal) * 5);

//         // for (int i = 0; i < 5; i++) {
//         //     std::cout << i << ") " << "normal: (" << normals[i].normal.x << ", " << normals[i].normal.y << ", " << normals[i].normal.z << ")" << std::endl;
//         // }
//     }

//     void gicp_step_gpu(PointCloud& source_point_cloud, SSBO& source_normals_ssbo) {
//         Location source_location;
//         source_location.position = glm::vec4(source_point_cloud.position, 1.0f);
//         source_location.rotation = glm::vec4(source_point_cloud.rotation, 1.0f);

//         source_location_ssbo.update_subdata(0, &source_location, sizeof(Location));

//         SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
//         std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
//         int x_count = math_utils::div_up_u32(num_source_points, 256);
        
//         map_hash_table_ssbo.bind_base(0);
        
//         map_points_ssbo.bind_base(1);
//         map_normals_ssbo.bind_base(2);
//         num_map_points_ssbo.bind_base(3);
        
//         source_points_ssbo.bind_base(4);
//         source_normals_ssbo.bind_base(5);
//         h_and_g_ssbo.bind_base(6);
//         source_location_ssbo.bind_base(7);
//         out_val_ssbo.bind_base(9);
//         target_ids_ssbo.bind_base(10);
//         output_hg.bind_base(11);
//         trace_ssbo.bind_base(12);
//         num_trace_points_ssbo.bind_base(13);
        
        
//         // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
//         align_point_cloud_program->use();
//         align_point_cloud_program->set_uint("num_source_points", num_source_points);
        
//         align_point_cloud_program->dispatch_compute(x_count, 1, 1);
//         glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//         num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

//         Location location_out = {};
//         source_location_ssbo.read_subdata(0u, &location_out, sizeof(Location));

//         source_point_cloud.position = location_out.position;
//         source_point_cloud.rotation = location_out.rotation;

//         std::cout << "position: (" << location_out.position.x << ", " << location_out.position.y << ", " << location_out.position.z << ")" << std::endl;
//         std::cout << "rotation: (" << location_out.rotation.x << ", " << location_out.rotation.y << ", " << location_out.rotation.z << ")" << std::endl;


//         glm::dvec4 out = {};
//         out_val_ssbo.read_subdata(0u, &out, sizeof(glm::dvec4));

        
//         std::cout << "bool ok = " << out.x << std::endl;

//         HAndG out_h_and_g = {};
//         output_hg.read_subdata(0u, &out_h_and_g, sizeof(HAndG));

//         std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);

//         std::cout << "double delta[6] = {";
//         for (int i = 0; i < 6; ++i) {
//             std::cout << out_h_and_g.g[i];
//             if (i < 5) std::cout << ", ";
//         }
//         std::cout << "};\n";

//         uint32_t num_trace_points = 0;
//         num_trace_points_ssbo.read_subdata(0u, &num_trace_points, sizeof(uint32_t));

//         TracePoint out_trace_points[num_trace_points];
//         trace_ssbo.read_subdata(0u, out_trace_points, sizeof(TracePoint) * num_trace_points);

//         std::ofstream out_stream("/home/spectre/Projects/test_open_3d/solve_6x6_gpu_dump.txt");
//         out_stream << std::setprecision(std::numeric_limits<double>::max_digits10);

//         for (int i = 0; i < num_trace_points; i++) {
//             out_stream << out_trace_points[i].counter << "." << out_trace_points[i].site_id << " = " << out_trace_points[i].value << '\n';
//         }
//         out_stream.close();
        
            

//         // for (int i = 0; i < 6; i++) {
//         //     for (int j = 0; j < 6; j++) {
//         //         std::cout << "h[" << i << "][" << j << "] = " << out_h_and_g.h[i][j] << std::endl;
//         //     }
//         // }

//         // std::cout << std::endl;

//         // for (int i = 0; i < 6; i++) {
//         //     std::cout << "g[" << i << "] = " << out_h_and_g.g[i] << std::endl;
//         // }
//     }

//     void align_point_cloud(PointCloud& source_point_cloud, SSBO& source_normals_ssbo) {
//         source_point_cloud.sync_gpu();
//         SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
//         std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
//         int x_count = math_utils::div_up_u32(num_source_points, 256);
        
//         map_hash_table_ssbo.bind_base(0);
        
//         map_points_ssbo.bind_base(1);
//         map_normals_ssbo.bind_base(2);
//         num_map_points_ssbo.bind_base(3);
        
//         source_points_ssbo.bind_base(4);
//         source_normals_ssbo.bind_base(5);
//         h_and_g_ssbo.bind_base(6);
        
//         // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
//         align_point_cloud_program->set_uint("num_source_points", num_source_points);

//         align_point_cloud_program->use();
//         align_point_cloud_program->dispatch_compute(x_count, 1, 1);
//         glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//         num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

//         HAndG out_h_and_g = {};
//         h_and_g_ssbo.read_subdata(0u, &out_h_and_g, sizeof(HAndG));

//         for (int i = 0; i < 6; i++) {
//             for (int j = 0; j < 6; j++) {
//                 std::cout << "h[" << i << "][" << j << "] = " << out_h_and_g.h[i][j] << std::endl;
//             }
//         }

//         std::cout << std::endl;

//         for (int i = 0; i < 6; i++) {
//             std::cout << "g[" << i << "] = " << out_h_and_g.g[i] << std::endl;
//         }
//     }

//     glm::mat3 test(PointCloud& source_point_cloud, SSBO& source_normals_ssbo, glm::vec3 euler) {
//         source_point_cloud.sync_gpu();
//         SSBO source_points_ssbo = SSBO(*source_point_cloud.point_renderer.instance_vbo);
//         std::uint32_t num_source_points = source_point_cloud.point_renderer.instance_count;
//         int x_count = math_utils::div_up_u32(num_source_points, 256);
        
//         map_hash_table_ssbo.bind_base(0);
        
//         map_points_ssbo.bind_base(1);
//         map_normals_ssbo.bind_base(2);
//         num_map_points_ssbo.bind_base(3);
        
//         source_points_ssbo.bind_base(4);
//         source_normals_ssbo.bind_base(5);
//         h_and_g_ssbo.bind_base(6);
//         Mat3Std430_ssbo.bind_base(9);
        
//         // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
//         align_point_cloud_program->set_uint("num_source_points", num_source_points);
//         align_point_cloud_program->set_vec3("uEuler", euler);

//         align_point_cloud_program->use();
//         align_point_cloud_program->dispatch_compute(x_count, 1, 1);
//         glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//         num_map_points_ssbo.read_subdata(0u, &num_map_points, sizeof(uint32_t));

//         Mat3Std430 out_Mat3Std430 = {};
//         Mat3Std430_ssbo.read_subdata(0u, &out_Mat3Std430, sizeof(Mat3Std430));

//         glm::mat3 result;
        
//         for (int i = 0; i < 3; i++) {
//             result[0][i] = out_Mat3Std430.col0[i];
//         }
//         // std::cout << std::endl;
//         for (int i = 0; i < 3; i++) {
//             result[1][i] = out_Mat3Std430.col1[i];
//         }
//         // std::cout << std::endl;
//         for (int i = 0; i < 3; i++) {
//             result[2][i] = out_Mat3Std430.col2[i];
//         }
//         // std::cout << std::endl;

//         return result;
//     }
// };




// void get_points(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, glm::vec4 color) {
//     // int size = 10;
//     // // float step = 0.2f; // distance between grid points
//     // float step = 2.0f; // distance between grid points
//     // for (int x = 0; x < size; x++) {
//     //     for (int z = 0; z < size; z++) {
//     //         PointInstance point;
//     //         point.color = color;

//     //         // Center the grid around (0, 0)
//     //         float px = (x - (size - 1) * 0.5f) * step;
//     //         float pz = (z - (size - 1) * 0.5f) * step;

//     //         // Surface: y = f(x, z)
//     //         float py =
//     //             0.15f * sinf(1.7f * px) +
//     //             0.10f * cosf(1.3f * pz) +
//     //             0.05f * px * pz;

//     //         point.pos = glm::vec4(px, py, pz, 1.0f);

//     //         // Partial derivatives of f(x, z)
//     //         float dfdx =
//     //             0.15f * 1.7f * cosf(1.7f * px) +
//     //             0.05f * pz;

//     //         float dfdz =
//     //         -0.10f * 1.3f * sinf(1.3f * pz) +
//     //             0.05f * px;

//     //         // For surface y = f(x,z), normal is (-df/dx, 1, -df/dz)
//     //         glm::vec3 n = glm::normalize(glm::vec3(-dfdx, 1.0f, -dfdz));
//     //         glm::vec4 normal = glm::vec4(n, 0.0f);
//     //         // glm::vec4 normal = n;

//     //         points.push_back(point);
//     //         normals.push_back(normal);
//     //     }
//     // }

//     PointInstance point;
//     point.pos = glm::vec4(0, 0, 0, 1);
//     point.color = color;

//     points.push_back(point);
//     normals.push_back(glm::vec4(0, 1, 0, 1));
// }

// void get_normals_ssbo(std::vector<glm::vec4> &normals, SSBO& normals_ssbo) {
//     normals_ssbo = SSBO(normals.size() * sizeof(glm::vec4), GL_DYNAMIC_DRAW, normals.data());
// }


// struct TraceLine {
//     unsigned long long counter;
//     unsigned long long site_id;
//     double value;
// };

// bool parse_line(const std::string& s, TraceLine& out) {
//     return std::sscanf(s.c_str(), "%llu.%llu = %lf",
//                        &out.counter, &out.site_id, &out.value) == 3;
// }

// bool close_enough(double a, double b, double abs_eps = 1e-3, double rel_eps = 1e-6) {
//     double diff = std::abs(a - b);
//     if (diff <= abs_eps) {
//         return true;
//     }

//     double scale = std::max(std::abs(a), std::abs(b));
//     return diff <= rel_eps * scale;
// }

void print_vec4(glm::vec4 input_vec) {
    std::cout << input_vec.x << " " << input_vec.y << " " << input_vec.z << " " << input_vec.w;
}

#include <vector>
#include <glm/glm.hpp>
#include <cmath>
#include <random>

#include <random>

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstdint>


static void push_point_with_normal(
    std::vector<PointInstance>& points,
    std::vector<glm::vec4>& normals,
    const glm::vec3& p,
    const glm::vec3& n,
    const glm::vec4& color
) {
    PointInstance point;
    point.pos = glm::vec4(p, 1.0f);
    point.color = color;
    points.push_back(point);
    normals.push_back(glm::vec4(glm::normalize(n), 0.0f));
}

static float lerp_f(float a, float b, float t) {
    return a + (b - a) * t;
}

static float smooth_f(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static float hash_2d(int x, int y, uint32_t seed) {
    uint32_t h = seed;
    h ^= hash_u32(static_cast<uint32_t>(x) + 0x9e3779b9U);
    h ^= hash_u32(static_cast<uint32_t>(y) + 0x85ebca6bU);
    h = hash_u32(h);
    return float(h) / float(0xFFFFFFFFu);
}

static float value_noise_2d(const glm::vec2& p, uint32_t seed) {
    int x0 = int(std::floor(p.x));
    int y0 = int(std::floor(p.y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float tx = p.x - float(x0);
    float ty = p.y - float(y0);

    float sx = smooth_f(tx);
    float sy = smooth_f(ty);

    float v00 = hash_2d(x0, y0, seed);
    float v10 = hash_2d(x1, y0, seed);
    float v01 = hash_2d(x0, y1, seed);
    float v11 = hash_2d(x1, y1, seed);

    float a = lerp_f(v00, v10, sx);
    float b = lerp_f(v01, v11, sx);
    return lerp_f(a, b, sy);
}

static float fbm_2d(glm::vec2 p, uint32_t seed) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int i = 0; i < 5; ++i) {
        value += amplitude * value_noise_2d(p * frequency, seed + uint32_t(i * 37));
        frequency *= 2.03f;
        amplitude *= 0.5f;
    }

    return value;
}

static float non_repeating_wave(const glm::vec2& uv, float wave_height, uint32_t seed) {
    glm::vec2 q = uv * 0.045f;

    glm::vec2 warp(
        fbm_2d(q + glm::vec2(17.3f, -9.1f), seed + 11u),
        fbm_2d(q + glm::vec2(-4.7f, 23.8f), seed + 29u)
    );

    q += 1.75f * (warp - glm::vec2(0.5f));

    float n1 = fbm_2d(q, seed + 101u);
    float n2 = fbm_2d(q * 1.91f + glm::vec2(8.2f, -3.7f), seed + 211u);

    float s1 = std::sin(uv.x * 0.173f + uv.y * 0.117f + 0.3f);
    float s2 = std::sin(uv.x * 0.071f - uv.y * 0.191f + 1.7f);
    float s3 = std::sin(std::sqrt(uv.x * uv.x + uv.y * uv.y) * 0.113f + 0.8f);

    float noise_part = ((n1 * 2.0f - 1.0f) * 0.65f) + ((n2 * 2.0f - 1.0f) * 0.35f);
    float wave_part = 0.45f * s1 + 0.35f * s2 + 0.20f * s3;

    return wave_height * (0.75f * noise_part + 0.25f * wave_part);
}

static void add_wavy_surface(
    std::vector<PointInstance>& target_points,
    std::vector<glm::vec4>& target_normals,
    std::vector<PointInstance>& source_points,
    std::vector<glm::vec4>& source_normals,
    const glm::vec3& origin,
    const glm::vec3& axis_u,
    const glm::vec3& axis_v,
    const glm::vec3& base_normal,
    float u_max,
    float v_max,
    float step,
    float wave_height,
    uint32_t seed,
    const glm::vec4& target_color,
    const glm::vec4& source_color
) {
    const float eps = step * 0.5f;

    for (float u = 0.0f; u <= u_max; u += step) {
        for (float v = 0.0f; v <= v_max; v += step) {
            glm::vec2 uv(u, v);

            float d  = non_repeating_wave(uv, wave_height, seed);
            float du = non_repeating_wave(glm::vec2(u + eps, v), wave_height, seed) - d;
            float dv = non_repeating_wave(glm::vec2(u, v + eps), wave_height, seed) - d;

            glm::vec3 p = origin + axis_u * u + axis_v * v + base_normal * d;

            glm::vec3 tangent_u = axis_u + base_normal * (du / eps);
            glm::vec3 tangent_v = axis_v + base_normal * (dv / eps);

            glm::vec3 n = glm::normalize(glm::cross(tangent_u, tangent_v));
            if (glm::dot(n, base_normal) < 0.0f) {
                n = -n;
            }

            push_point_with_normal(target_points, target_normals, p, n, target_color);
            push_point_with_normal(source_points, source_normals, p, n, source_color);
        }
    }
}

void generate_half_box_with_sphere(
    std::vector<PointInstance>& target_points,
    std::vector<PointInstance>& source_points,
    std::vector<glm::vec4>& target_normals,
    std::vector<glm::vec4>& source_normals,
    float wave_height = 3.0f
) {
    target_points.clear();
    source_points.clear();
    target_normals.clear();
    source_normals.clear();

    const float box_size = 300.0f;
    const float wall_height = box_size * 0.5f;
    const float step = 2.0f;

    const glm::vec4 target_box_color(0, 0, 1, 1);
    const glm::vec4 source_box_color(1, 0, 0, 1);

    const glm::vec4 target_sphere_color(0, 1, 1, 1);
    const glm::vec4 source_sphere_color(1, 1, 0, 1);

    // Floor
    // add_wavy_surface(
    //     target_points, target_normals,
    //     source_points, source_normals,
    //     glm::vec3(0.0f, 0.0f, 0.0f),
    //     glm::vec3(1.0f, 0.0f, 0.0f),
    //     glm::vec3(0.0f, 0.0f, 1.0f),
    //     glm::vec3(0.0f, 1.0f, 0.0f),
    //     box_size, box_size, step, wave_height, 1001u,
    //     target_box_color, source_box_color
    // );

    // // Left wall (x = 0, inward normal +X)
    // add_wavy_surface(
    //     target_points, target_normals,
    //     source_points, source_normals,
    //     glm::vec3(0.0f, 0.0f, 0.0f),
    //     glm::vec3(0.0f, 1.0f, 0.0f),
    //     glm::vec3(0.0f, 0.0f, 1.0f),
    //     glm::vec3(1.0f, 0.0f, 0.0f),
    //     wall_height, box_size, step, wave_height, 2001u,
    //     target_box_color, source_box_color
    // );

    // // Right wall (x = box_size, inward normal -X)
    // add_wavy_surface(
    //     target_points, target_normals,
    //     source_points, source_normals,
    //     glm::vec3(box_size, 0.0f, 0.0f),
    //     glm::vec3(0.0f, 1.0f, 0.0f),
    //     glm::vec3(0.0f, 0.0f, 1.0f),
    //     glm::vec3(-1.0f, 0.0f, 0.0f),
    //     wall_height, box_size, step, wave_height, 3001u,
    //     target_box_color, source_box_color
    // );

    // // Front wall (z = 0, inward normal +Z)
    // add_wavy_surface(
    //     target_points, target_normals,
    //     source_points, source_normals,
    //     glm::vec3(0.0f, 0.0f, 0.0f),
    //     glm::vec3(1.0f, 0.0f, 0.0f),
    //     glm::vec3(0.0f, 1.0f, 0.0f),
    //     glm::vec3(0.0f, 0.0f, 1.0f),
    //     box_size, wall_height, step, wave_height, 4001u,
    //     target_box_color, source_box_color
    // );

    // // Back wall (z = box_size, inward normal -Z)
    // add_wavy_surface(
    //     target_points, target_normals,
    //     source_points, source_normals,
    //     glm::vec3(0.0f, 0.0f, box_size),
    //     glm::vec3(1.0f, 0.0f, 0.0f),
    //     glm::vec3(0.0f, 1.0f, 0.0f),
    //     glm::vec3(0.0f, 0.0f, -1.0f),
    //     box_size, wall_height, step, wave_height, 5001u,
    //     target_box_color, source_box_color
    // );

    // Sphere inside
    const glm::vec3 sphere_center(box_size * 0.5f, wall_height * 0.5f, box_size * 0.5f);
    const float sphere_radius = box_size * 0.18f;

    const int lat_steps = 48;
    const int lon_steps = 96;

    for (int lat = 0; lat <= lat_steps; ++lat) {
        float v = float(lat) / float(lat_steps);
        float theta = glm::pi<float>() * v;

        for (int lon = 0; lon < lon_steps; ++lon) {
            float u = float(lon) / float(lon_steps);
            float phi = 2.0f * glm::pi<float>() * u;

            glm::vec3 n;
            n.x = std::sin(theta) * std::cos(phi);
            n.y = std::cos(theta);
            n.z = std::sin(theta) * std::sin(phi);

            glm::vec3 p = sphere_center + sphere_radius * n;

            push_point_with_normal(target_points, target_normals, p, n, target_sphere_color);
            push_point_with_normal(source_points, source_normals, p, n, source_sphere_color);
        }
    }
}


int main() {
    VulkanEngine engine;
    VulkanWindow window(&engine, 1280, 720, "3D visualization");
    engine.set_vulkan_window(&window);

    ui::VulkanInitInfo ui_info{};
    ui_info.instance = engine.instance;
    ui_info.physical_device = engine.physicalDevice;
    ui_info.device = engine.device;
    ui_info.queue_family = engine.get_graphics_queue_family();
    ui_info.queue = engine.graphicsQueue;
    ui_info.descriptor_pool = engine.get_imgui_descriptor_pool();
    ui_info.render_pass = engine.renderPass;
    ui_info.min_image_count = engine.get_imgui_min_image_count();
    ui_info.image_count = static_cast<uint32_t>(engine.swapchainImages.size());
    ui_info.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    ui_info.pipeline_cache = VK_NULL_HANDLE;
    ui_info.subpass = 0;
    ui_info.check_vk_result_fn = VulkanEngine::imgui_check_vk_result;

    ui::init(window.window, ui_info);


    // VkPhysicalDeviceFeatures supported{};
    // vkGetPhysicalDeviceFeatures(engine.physicalDevice, &supported);

    // if (!supported.shaderFloat64) {
    //     throw std::runtime_error("GPU does not support shaderFloat64");
    // }

    // VkPhysicalDeviceFeatures enabled{};
    // enabled.shaderFloat64 = VK_TRUE;

    // VkDeviceCreateInfo createInfo{};
    // createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    // createInfo.pEnabledFeatures = &enabled;

    // // fill queue infos, extensions, etc.

    // VkDevice device = VK_NULL_HANDLE;
    // VkResult res = vkCreateDevice(engine.physicalDevice, &createInfo, nullptr, &device);
    // if (res != VK_SUCCESS) {
    //     throw std::runtime_error("vkCreateDevice failed");
    // }


    Camera camera = Camera();
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 20;

    ShaderModule vert_module = ShaderModule(engine.device, "shaders/pbr.vert.spv");
    ShaderModule frag_module = ShaderModule(engine.device, "shaders/pbr.frag.spv");

    ShaderModule equirect_to_cubemap_vs = ShaderModule(engine.device, "shaders/equirect_to_cubemap.vert.spv");
    ShaderModule equirect_to_cubemap_fs = ShaderModule(engine.device, "shaders/equirect_to_cubemap.frag.spv");

    PBRRenderer renderer = PBRRenderer(engine, vert_module, frag_module, 0.3f);

    Mesh mesh = create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    Mesh sphere_2 = create_sphere_mesh(engine, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    sphere_2.position.x = 2;
    std::vector<Mesh> spheres;

    for (int x = -5; x < 5; x++)
        for (int z = -5; z < 5; z++) {
            // Mesh sphere_mesh = create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

            // sphere_mesh.position = glm::vec3(x, 0, z);

            spheres.push_back(create_sphere_mesh(engine, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
            spheres.back().position = glm::vec3(x, 0, z);
        }


    EquirectToCubemapPass equirect_to_cubemap_pass(engine);
    BrdfLutGenerater brdf_lut_generator(engine);
    IrradianceMapGenerator irradiance_map_generator(engine);
    PrefilterMapGenerator prefilter_map_generator(engine);

    ResourceLoader resource_loader;
    resource_loader.create(engine, 154217728);

    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/st_peters_square_night_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    // Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/citrus_orchard_puresky_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    Texture2D equirectangular_map = resource_loader.load_hdr_texture2d("assets/hdr/studio_kominka_02_4k.hdr", 6, VK_IMAGE_USAGE_SAMPLED_BIT);
    
    Texture2D brdf_lut = brdf_lut_generator.generate(256, 256);
    
    Cubemap environment_map = equirect_to_cubemap_pass.generate(equirectangular_map, 1920);
    Cubemap irradiance_map = irradiance_map_generator.generate(environment_map, 256);
    Cubemap prefilter_map = prefilter_map_generator.generate(environment_map, 256);

    SkyboxPass skybox_pass;
    skybox_pass.create(engine);

    LightingSystem lighting_system;
    lighting_system.init(engine);

    LightSource red_light_source{glm::vec4(1, 2, 0, 10), glm::vec4(1, 0, 0, 1)};
    LightSource green_light_source{glm::vec4(0, 2, 1, 10), glm::vec4(0, 1, 0, 1)};
    LightSource blue_light_source{glm::vec4(-1, 2, 0, 10), glm::vec4(0, 0, 1, 1)};

    // light_source.position = glm::vec4(0, 2, 0, 10);
    // light_source.color = glm::vec4(1, 0, 0, 1);

    lighting_system.set_light_source(0, red_light_source);
    lighting_system.set_light_source(1, green_light_source);
    lighting_system.set_light_source(2, blue_light_source);

    PointCloudPass point_cloud_pass;
    point_cloud_pass.create(engine);

    // PointCloud point_cloud;
    // point_cloud.create(engine);


    // std::vector<PointInstance> point_instances;

    // for (int i = 0; i < 100; i++) {
    //     PointInstance point;
    //     point.pos = glm::vec4(i, 0, 0, 1);
    //     point.color = glm::vec4(i / 100.0f, i / 100.0f, 1.0f - i / 100.0f, 1);

    //     point_instances.push_back(point);
    // }

    // point_cloud.set_points(point_instances);
    
    std::vector<PointInstance> target_points{}; 
    std::vector<PointInstance> source_points{};

    std::vector<glm::vec4> target_normals{};
    std::vector<glm::vec4> source_normals{};

    // for (int x = 0; x < 316; x++)
    //     for (int z = 0; z < 316; z++) {
    //         PointInstance point;
    //         glm::vec4 normal = glm::vec4(0, 1, 0, 1);

    //         point.pos = glm::vec4(x, 0, z, 1.0f);
    //         point.color = glm::vec4(0, 0, 1, 1);

    //         target_points.push_back(point);

    //         point.color = glm::vec4(1, 0, 0, 1);

    //         source_points.push_back(point);
            
    //         target_normals.push_back(normal);
    //         source_normals.push_back(normal);
    //     }

    // generate_half_box_with_sphere(target_points, source_points, target_normals, source_normals, 30.0f);


    
    // PointCloudFrame source_point_cloud_frame;
    // PointCloudFrame target_point_cloud_frame;

    // target_point_cloud_frame.points = target_points;
    // source_point_cloud_frame.points = source_points;

    // target_point_cloud_frame.normals = target_normals;
    // source_point_cloud_frame.normals = source_normals;

    // target_point_cloud_frame.point_cloud.create(engine);
    // source_point_cloud_frame.point_cloud.create(engine);

    // target_point_cloud_frame.point_cloud.set_points(target_points);
    // source_point_cloud_frame.point_cloud.set_points(source_points);

    PointCloudVideo point_cloud_video = PointCloudVideo();
    point_cloud_video.load_from_file(engine, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 2);

    PointCloudFrame& source_point_cloud_frame = point_cloud_video.frames[0];
    PointCloudFrame& target_point_cloud_frame = point_cloud_video.frames[1];

    source_point_cloud_frame.point_cloud.position += glm::vec3(3, 3, 0);
    source_point_cloud_frame.point_cloud.rotation = glm::vec3(0.2f, 0, 0);

    for (int i = 0; i < source_point_cloud_frame.points.size(); i++) {
        source_point_cloud_frame.points[i].color = glm::vec4(1, 0, 0, 1);
    }
    source_point_cloud_frame.point_cloud.set_points(source_point_cloud_frame.points);

    // VideoBuffer target_normal_buffer;
    // VideoBuffer source_normal_buffer;

    // target_normal_buffer.create(engine, target_normals.size() * sizeof(glm::vec4));
    // source_normal_buffer.create(engine, source_normals.size() * sizeof(glm::vec4));

    // target_normal_buffer.update_data(target_normals.data(), target_normals.size() * sizeof(glm::vec4));
    // source_normal_buffer.update_data(source_normals.data(), source_normals.size() * sizeof(glm::vec4));

    VoxelPointMap voxel_point_map;
    voxel_point_map.create(engine, 1500000, 1500000);

    VoxelPointMapReseter voxel_point_map_reseter;
    voxel_point_map_reseter.create(engine);

    voxel_point_map_reseter.reset(voxel_point_map);

    VoxelMapPointInserter voxel_map_point_inserter;
    voxel_map_point_inserter.create(engine);

    target_point_cloud_frame.point_cloud.position -= glm::vec3(0, 0, 0);
    target_point_cloud_frame.point_cloud.rotation = glm::vec3(0.0f, 0, 0);

    voxel_map_point_inserter.insert(voxel_point_map, target_point_cloud_frame.point_cloud, target_point_cloud_frame.normal_buffer);

    GICPPass gicp_pass;
    gicp_pass.create(engine);

    
    PointCloud voxel_map_point_cloud;
    voxel_map_point_cloud.create(engine);

    voxel_map_point_cloud.set_points(voxel_point_map.map_point_buffer, voxel_point_map.map_point_count);

    // for (int i = 0; i < 10; i++) {
    //     GICP::step(source_point_cloud_frame, target_point_cloud_frame, source_point_cloud_frame.normals, target_point_cloud_frame.normals);
    // }

    float timer = 0.0f;
    float last_frame = 0.0f;
    while (window.is_open()) {
        engine.poll_events();

        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        last_frame = currentFrame;

        camera_controller.update(&window, delta_time);

        red_light_source.position.y = sin(timer * 2 + 0.234f) * 2;
        green_light_source.position.x = sin(timer * 2 + 1.3423f) * 2;
        blue_light_source.position.z = cos(timer * 2) * 2;

        engine.begin_frame(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));
        if (!engine.frameInProgress) {
            continue;
        }

        // skybox_pass.render(camera, environment_map);

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        lighting_system.update(camera);

        point_cloud_pass.render(source_point_cloud_frame.point_cloud, camera);
        // point_cloud_pass.render(target_point_cloud_frame.point_cloud, camera);
        point_cloud_pass.render(voxel_map_point_cloud, camera);
        

        ImGui::Begin("Hello");

        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        if (ImGui::Button("GICP step")) {
            // GICP::step(source_point_cloud_frame, target_point_cloud_frame, source_point_cloud_frame.normals, target_point_cloud_frame.normals);
            // GICP::step_test(source_point_cloud_frame, target_point_cloud_frame, source_point_cloud_frame.normals, target_point_cloud_frame.normals);
            // gicp_pass.step(source_point_cloud_frame.point_cloud, target_point_cloud_frame.point_cloud, source_normal_buffer, target_normal_buffer);
            gicp_pass.step(voxel_point_map, source_point_cloud_frame.point_cloud, source_point_cloud_frame.normal_buffer);

            std::cout << "position: (";
            print_vec4(glm::vec4(source_point_cloud_frame.point_cloud.position, 1.0f));
            std::cout << ")     rotation: (";
            print_vec4(glm::vec4(source_point_cloud_frame.point_cloud.rotation, 1.0f));
            std::cout << ")" << std::endl;
        }

        ImGui::End();

        ui::end_frame(engine.currentCommandBuffer);
        engine.end_frame();

        timer += delta_time;
    }

    vkDeviceWaitIdle(engine.device);
    ui::shutdown();
}

