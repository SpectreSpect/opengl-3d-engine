// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include "math_utils.h"
#include "light_source.h"
#include "circle_cloud.h"
#include "texture.h"
#include "cubemap.h"
#include "skybox.h"
#include "framebuffer.h"
#include "sphere.h"
#include "cube.h"
#include "quad.h"
#include "texture_manager.h"
#include "pbr_skybox.h"
#include <algorithm>
#include <random>
#include "gicp.h"

struct alignas(16) HashPoint {
    glm::vec4 position;
    glm::vec4 normal;
};

struct alignas(16) HashTableSlot {
    glm::uvec2     hash_key;
    std::uint32_t  _pad0[2];     // forces hash_value to offset 16

    HashPoint      hash_value;   // offset 16, size 32
    std::uint32_t  hash_state;   // offset 48
    std::uint32_t  _pad1[3];     // tail pad so sizeof == 64
};



bool next_pos(glm::ivec3 origin, int layer, glm::ivec3& index_pos, glm::ivec3& out_pos) {
    const int dim = 1 + layer * 2;

    // current position
    int x = index_pos.x;
    int y = index_pos.y;
    int z = index_pos.z;

    // emit current position first
    out_pos = origin + glm::ivec3(x, y, z) - glm::ivec3(layer);

    // advance to next position
    if ((z == 0 || z == dim - 1) || (y == 0 || y == dim - 1)) {
        x++;
    } else {
        if (x == dim - 1)
            x++;
        else
            x = dim - 1;
    }

    if (x >= dim) {
        x = 0;
        z++;

        if (z >= dim) {
            z = 0;
            y++;
        }
    }

    index_pos = glm::ivec3(x, y, z);

    return (y >= dim);
}


float clear_col[4] = {0.2f, 0.2f, 0.2f, 1.0f};

int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;


    // uint32_t u_hash_table_size = 100000;

    // SSBO hash_table_ssbo = SSBO::from_fill(sizeof(std::uint32_t) * 4 + sizeof(HashTableSlot) * u_hash_table_size, GL_DYNAMIC_DRAW, 0u, *engine.shader_manager); 

    // ComputeProgram test_hash_table_program = ComputeProgram(&engine.shader_manager->test_hash_table_cs);
    // ComputeProgram add_point_cloud_to_map_program = ComputeProgram(&engine.shader_manager->add_point_cloud_to_map_cs);

    



    // hash_table_ssbo.bind_base(0);

    // // hash_table_size
    // test_hash_table_program.set_uint("u_hash_table_size", u_hash_table_size);

    // test_hash_table_program.use();
    // test_hash_table_program.dispatch_compute(1, 1, 1);
    // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


    // HashTableSlot hash_table[u_hash_table_size] = {};
    // hash_table_ssbo.read_subdata(sizeof(std::uint32_t) * 4, hash_table, sizeof(HashTableSlot) * (u_hash_table_size - 1));

    // for (int i = 0; i < u_hash_table_size; i++) {
    //     std::cout << "(" << hash_table[i].hash_value.x << " " << hash_table[i].hash_value.y << " " << hash_table[i].hash_value.z << ")" << std::endl;
    // }

    std::vector<LineInstance> layer_line_instances;
    std::vector<LineInstance> layer_line_instances_blue;
    // layer_line_instances.push_back({glm::vec3(0, 0, 0), glm::vec3(5, 0, 0)});
    Line layer_line;
    Line layer_line_blue;
    layer_line_blue.color = glm::vec4(0, 0, 1, 1);

    // for (int layer = 0; layer < 3; layer++) {
    //     int x = 0;
    //     int y = 0;
    //     int z = 0;

    //     int x_prev = 0;
    //     int y_prev = 0;
    //     int z_prev = 0;

    //     float dim_size = 1 + layer * 2;
    //     while(x < dim_size) {
    //         while (y < dim_size) {
    //             while (z < dim_size) {
    //                 if (x != 0 && y != 0 && z != 0) {
    //                     LineInstance line_instance;

    //                     line_instance.p0 = glm::vec3(x_prev, y_prev, z_prev);
    //                     line_instance.p1 = glm::vec3(x, y, z);

    //                     layer_line_instances.push_back(line_instance);

    //                     x_prev = x;
    //                     y_prev = y;
    //                     z_prev = z;
    //                 }
                    
    //                 if ((x != 0) || (x != (dim_size - 1)) && (y != 0) || (y != (dim_size - 1))) {
    //                     if (z == dim_size - 1)
    //                         z += 1;
    //                     else
    //                         z = dim_size - 1;
    //                 }
    //                 else
    //                     z += 1;
    //             }

    //             if ((x != 0) || (x != (dim_size - 1)) && (z != 0) || (z != (dim_size - 1))) {
    //                 if (y == dim_size - 1)
    //                     y += 1;
    //                 else
    //                     y = dim_size - 1;
    //             }
    //             else
    //                 y += 1;
    //         }

    //         if ((z != 0) || (z != (dim_size - 1)) && (y != 0) || (y != (dim_size - 1))) {
    //             if (x == dim_size - 1)
    //                 x += 1;
    //             else
    //                 x = dim_size - 1;
    //         }
    //         else
    //             x += 1;
    //     }
    // }

    // glm::ivec3 voxel_origin = glm::ivec3(0, 0, 0);
    // glm::ivec3 index_pos = glm::ivec3(0, 0, 0);
    // for (int layer = 0; layer < 3; layer++) {
    //     glm::ivec3 cur_pos = glm::ivec3(0, 0, 0);
    //     bool finished = false;
    //     while (!finished) {
    //         finished = next_pos(voxel_origin, layer, index_pos, cur_pos);

    //         std::cout << "(" << cur_pos.x << ", " << cur_pos.y << ", " << cur_pos.z << ")" << std::endl;

    //         LineInstance line_instance;
    //         line_instance.p0 = cur_pos;
    //         line_instance.p1 = cur_pos;
    //         line_instance.p1.x += 0.2f;
    //     }
    // }


    glm::ivec3 voxel_origin(0, 0, 0);

    // for (int layer = 2; layer < 3; layer++) {
    //     glm::ivec3 index_pos(0, 0, 0); // reset every layer
    //     glm::ivec3 cur_pos(0, 0, 0);

    //     bool finished = false;
    //     while (!finished) {
    //         finished = next_pos(voxel_origin, layer, index_pos, cur_pos);

    //         std::cout << "(" << cur_pos.x << ", " << cur_pos.y << ", " << cur_pos.z << ")\n";
    //         LineInstance line_instance;
    //         line_instance.p0 = cur_pos;
    //         line_instance.p1 = cur_pos;
    //         line_instance.p1.x += 0.2f;

    //         layer_line_instances.push_back(line_instance);
    //     }
    // }

    for (int layer = 0; layer < 3; layer++) {
    const int dim = 1 + layer * 2;

        for (int y = 0; y < dim; y++) {
            const bool on_y_edge = (y == 0 || y == dim - 1);

            for (int z = 0; z < dim; z++) {
                const bool on_z_edge = (z == 0 || z == dim - 1);

                int x = 0;
                while (x < dim) {
                    glm::ivec3 pos = voxel_origin + glm::ivec3(x, y, z) - glm::ivec3(layer);

                    LineInstance line_instance;
                    line_instance.p0 = pos;
                    line_instance.p1 = pos;
                    line_instance.p1.x += 0.5f;

                    if (layer % 2 == 0)
                        layer_line_instances.push_back(line_instance);
                    else
                        layer_line_instances_blue.push_back(line_instance);

                    if (on_y_edge || on_z_edge) {
                        x++;
                    } else {
                        x = (x == dim - 1) ? x + 1 : dim - 1;
                    }
                }
            }
        }
    }

    PointCloudVideo point_cloud_video = PointCloudVideo();
    point_cloud_video.load_from_file("/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 5);
    
    // ComputeProgram test_hash_table_program = ComputeProgram(&engine.shader_manager->test_hash_table_cs);
    ComputeProgram add_point_cloud_to_map_program = ComputeProgram(&engine.shader_manager->add_point_cloud_to_map_cs);

    uint32_t u_hash_table_size = 100000;
    int max_map_points_count = 5000;



    SSBO hash_table_ssbo = SSBO::from_fill(sizeof(std::uint32_t) * 4 + sizeof(HashTableSlot) * u_hash_table_size, GL_DYNAMIC_DRAW, 0u, *engine.shader_manager); 

    point_cloud_video.frames[0].point_cloud.sync_gpu();
    SSBO source_point_ssbo = SSBO(*point_cloud_video.frames[0].point_cloud.point_renderer.instance_vbo);
    

    
    SSBO map_point_ssbo = SSBO::from_fill(sizeof(PointInstance) * (max_map_points_count), GL_DYNAMIC_DRAW, 0u, *engine.shader_manager);
    SSBO num_point_ssbo = SSBO::from_fill(sizeof(uint32_t), GL_DYNAMIC_DRAW, 0u, *engine.shader_manager);

    // std::uint32_t num_points = point_cloud_video.frames[0].point_cloud.points.size();
    std::uint32_t num_source_points = point_cloud_video.frames[0].point_cloud.point_renderer.instance_count;
    int x_count = math_utils::div_up_u32(num_source_points, 256);
    
    hash_table_ssbo.bind_base(0);
    map_point_ssbo.bind_base(1);
    num_point_ssbo.bind_base(2);
    source_point_ssbo.bind_base(3);
    // // add_point_cloud_to_map_program.set_uint("num_points", num_points);
    add_point_cloud_to_map_program.set_uint("num_source_points", num_source_points);

    add_point_cloud_to_map_program.use();
    add_point_cloud_to_map_program.dispatch_compute(x_count, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // int read_num = 4;

    // HashTableSlot hash_table[u_hash_table_size] = {};
    // hash_table_ssbo.read_subdata(sizeof(std::uint32_t) * 4, hash_table, sizeof(HashTableSlot) * (read_num - 1));

    // for (int i = 0; i < read_num; i++) {
    //     std::cout << "(" << hash_table[i].hash_value.position.x << " " << hash_table[i].hash_value.position.y << " " << hash_table[i].hash_value.position.z << ")" << std::endl;
    // }

    // PointInstance points[max_points_count] = {};
    // map_point_ssbo.read_subdata(0u, points, sizeof(PointInstance) * max_points_count);

    // for (int i = 0; i < read_num; i++) {
    //     std::cout << "(" << points[i].pos.x << " " << points[i].pos.y << " " << points[i].pos.z << ")" << std::endl;
    //     // std::cout << points[i] << std::endl;
    // }

    uint32_t output_num_points;
    num_point_ssbo.read_subdata(0u, &output_num_points, sizeof(uint32_t));



    layer_line.set_lines(layer_line_instances);
    layer_line_blue.set_lines(layer_line_instances_blue);

    Point map_points(map_point_ssbo, output_num_points);
    // map_points.instance_count = 4;
    


    
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    int cur_frame = 100;
    bool map_initialized = false;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // window.draw(&layer_line, &camera);
        // window.draw(&layer_line_blue, &camera);
        window.draw(&map_points, &camera);

        
        



        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);

        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
