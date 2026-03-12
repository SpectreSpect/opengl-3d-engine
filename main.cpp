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

// float clear_col[4] = {0.15f, 0.15f, 0.18f, 1.0f};
// float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};
float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};

void set_light_position(SSBO& light_source_ssbo, size_t index, const glm::vec4 &pos) {
    size_t offset = index * sizeof(LightSource);
    light_source_ssbo.update_subdata(offset, &pos, sizeof(pos));
}

int find_closest_id(const std::vector<PointInstance>& target_points,
                    const glm::vec3& point,
                    float max_dist_sq)
{
    if (target_points.empty())
        return -1;

    int min_id = -1;
    float min_dist = max_dist_sq;

    for (size_t i = 0; i < target_points.size(); i++) {
        float dist = glm::distance2(glm::vec3(target_points[i].pos), point);
        if (dist < min_dist) {
            min_dist = dist;
            min_id = (int)i;
        }
    }

    return min_id;
}


bool solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]) {
    // Augmented matrix [H | g], size 6 x 7
    double a[6][7];

    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 6; c++) {
            a[r][c] = H_in[r][c];
        }
        a[r][6] = g_in[r];
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < 6; col++) {
        // Find pivot row
        int pivot_row = col;
        double max_abs = std::abs(a[col][col]);

        for (int r = col + 1; r < 6; r++) {
            double v = std::abs(a[r][col]);
            if (v > max_abs) {
                max_abs = v;
                pivot_row = r;
            }
        }

        // Singular / degenerate check
        if (max_abs < 1e-12) {
            return false;
        }

        // Swap rows if needed
        if (pivot_row != col) {
            for (int c = col; c < 7; c++) {
                std::swap(a[col][c], a[pivot_row][c]);
            }
        }

        // Eliminate rows below
        for (int r = col + 1; r < 6; r++) {
            double factor = a[r][col] / a[col][col];

            for (int c = col; c < 7; c++) {
                a[r][c] -= factor * a[col][c];
            }
        }
    }

    // Back substitution
    for (int r = 5; r >= 0; r--) {
        double sum = a[r][6];

        for (int c = r + 1; c < 6; c++) {
            sum -= a[r][c] * delta_out[c];
        }

        if (std::abs(a[r][r]) < 1e-12) {
            return false;
        }

        delta_out[r] = sum / a[r][r];
    }

    return true;
}

glm::mat3 omega_to_mat3(const glm::vec3& omega) {
    float theta = glm::length(omega);

    if (theta < 1e-12f) {
        return glm::mat3(1.0f);
    }

    glm::vec3 axis = omega / theta;
    glm::mat4 R4 = glm::rotate(glm::mat4(1.0f), theta, axis);
    return glm::mat3(R4);
}

void icp_step(const std::vector<PointInstance>& source_points, 
              const std::vector<PointInstance>& target_points, const std::vector<glm::vec3>& target_normals, 
              glm::mat3& R, glm::vec3& t) { 

    float max_corr_dist = 20.0f;
    float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    float max_rot = glm::radians(5.0f);
    float max_trans = 5.0f;

    double H[6][6] = {};
    double g[6] = {};

    double total_sq_error = 0.0;
    int valid_count = 0;
    for (size_t i = 0; i < source_points.size(); i++) {
        // int target_id = correspondences[i];
        glm::vec3 p = glm::vec3(source_points[i].pos);
        glm::vec3 x = R * p + t;
        
        int target_id = find_closest_id(target_points, x, max_corr_dist_sq);
        if (target_id < 0) {
            continue;
        }
        // int target_id = (int)i;
        
        glm::vec3 q = glm::vec3(target_points[target_id].pos);
        glm::vec3 n = glm::normalize(target_normals[target_id]);

        glm::vec3 a = glm::cross(x, n);
        double rhs = (double)glm::dot(n, q - x);

        total_sq_error += rhs * rhs;

        double J[6] = {
            (double)a.x,
            (double)a.y,
            (double)a.z,
            (double)n.x,
            (double)n.y,
            (double)n.z
        };

        for (int r = 0; r < 6; r++) {
            g[r] += J[r] * rhs;

            for (int c = 0; c < 6; c++) {
                H[r][c] += J[r] * J[c];
            }
        }

        valid_count++;
    }

    if (valid_count < 6) {
        std::cout << "Too few correspondences\n";
        return;
    }

    double rmse = std::sqrt(total_sq_error / (double)valid_count);

    // damping
    double lambda = 1e-6;
    for (int i = 0; i < 6; i++) {
        H[i][i] += lambda;
    }

    double delta[6] = {};
    bool ok = solve_6x6(H, g, delta);

    if (!ok) {
        std::cout << "solve_6x6 failed\n";
        return;
    }

    glm::vec3 omega(
        (float)delta[0],
        (float)delta[1],
        (float)delta[2]
    );

    glm::vec3 v(
        (float)delta[3],
        (float)delta[4],
        (float)delta[5]
    );

    float omega_len = glm::length(omega);
    if (omega_len > max_rot) {
        omega *= max_rot / omega_len;
    }

    float v_len = glm::length(v);
    if (v_len > max_trans) {
        v *= max_trans / v_len;
    }

    glm::mat3 dR = omega_to_mat3(omega);

    R = dR * R;
    t = dR * t + v;

    std::cout << "valid_count = " << valid_count
          << ", rmse = " << rmse
          << ", |omega| = " << glm::length(omega)
          << ", |v| = " << glm::length(v)
          << "\n";
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    Camera camera;
    window.set_camera(&camera);
    FPSCameraController camera_controller = FPSCameraController(&camera);
    
    camera_controller.speed = 50;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 10, 24});
    voxel_grid->update(&window, &camera);
    sleep(1);

    glm::vec3 origin = glm::vec3(0.0f, 10.0f, 0.0f);

    std::vector<Line*> point_cloud_video_lines;
    std::vector<Point*> point_cloud_video_points;
    std::vector<Mesh*> lidar_meshes;

    PointCloudFrame point_cloud_frame = PointCloudFrame("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");
    size_t rings_count = 16;
    size_t ring_size = point_cloud_frame.point_cloud.size() / rings_count;
    
    Mesh point_cloud_mesh = point_cloud_frame.point_cloud.generate_mesh_gpu(rings_count, ring_size, 1.5);


    std::vector<PointInstance> target_points;
    std::vector<glm::vec3> target_normals;

    std::vector<PointInstance> source_points;
    std::vector<PointInstance> source_points_for_drawing;

    // Small known transform used to generate source from target
    glm::vec3 source_rotation = glm::radians(glm::vec3(5.0f, -3.0f, 2.0f)); // pitch, yaw, roll-ish
    glm::vec3 source_translation = glm::vec3(12.0f, -4.0f, 7.0f);

    // Build rotation matrix once
    glm::mat4 R4 = glm::yawPitchRoll(
        source_rotation.y, // yaw   around Y
        source_rotation.x, // pitch around X
        source_rotation.z  // roll  around Z
    );
    glm::mat3 initial_R = glm::mat3(R4);

    size_t size_x = 50;
    size_t size_z = 50;
    std::mt19937 rng(1337);

    // Gaussian noise
    std::normal_distribution<float> tangent_noise_dist(0.0f, 0.18f); // lateral jitter
    std::normal_distribution<float> normal_noise_dist(0.0f, 0.05f);  // smaller normal jitter

    // Random dropout
    std::uniform_real_distribution<float> keep_dist(0.0f, 1.0f);
    float keep_probability = 0.88f; // keep ~88% of source points

    float amplitude = static_cast<float>(size_x) / 8.0f;

    for (int x = 0; x < size_x; x++) {
        for (int z = 0; z < size_z; z++) {
            PointInstance new_point{};

            new_point.pos.x = static_cast<float>(x);
            new_point.pos.z = static_cast<float>(z);
            new_point.pos.w = 1.0f;

            // Normalize grid coords to [-1, 1]
            float u = 2.0f * static_cast<float>(x) / static_cast<float>(size_x - 1) - 1.0f;
            float v = 2.0f * static_cast<float>(z) / static_cast<float>(size_z - 1) - 1.0f;

            // Two asymmetric Gaussian features
            float du1 = u - 0.35f;
            float dv1 = v + 0.20f;
            float e1 = std::exp(-(du1 * du1 + dv1 * dv1) / 0.08f);

            float du2 = u + 0.45f;
            float dv2 = v - 0.35f;
            float e2 = std::exp(-(du2 * du2 + dv2 * dv2) / 0.05f);

            // Non-repeating asymmetric height function
            float h =
                0.35f * u
                + 0.18f * v
                + 0.55f * u * u
                - 0.30f * v * v
                + 0.22f * u * v
                + 0.65f * e1
                - 0.40f * e2;

            new_point.pos.y = h * amplitude;
            new_point.color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

            target_points.push_back(new_point);

            // --- Analytic normal ---
            float du_dx = 2.0f / static_cast<float>(size_x - 1);
            float dv_dz = 2.0f / static_cast<float>(size_z - 1);

            float df_du =
                0.35f
                + 1.10f * u
                + 0.22f * v
                + 0.65f * e1 * (-2.0f * du1 / 0.08f)
                - 0.40f * e2 * (-2.0f * du2 / 0.05f);

            float df_dv =
                0.18f
                - 0.60f * v
                + 0.22f * u
                + 0.65f * e1 * (-2.0f * dv1 / 0.08f)
                - 0.40f * e2 * (-2.0f * dv2 / 0.05f);

            float dy_dx = amplitude * df_du * du_dx;
            float dy_dz = amplitude * df_dv * dv_dz;

            glm::vec3 normal(-dy_dx, 1.0f, -dy_dz);
            normal = glm::normalize(normal);

            target_normals.push_back(normal);

            // --------------------------
            // Build perturbed source point
            // --------------------------

            // Optional dropout: source scan sees only part of the surface samples
            if (keep_dist(rng) > keep_probability) {
                continue;
            }

            PointInstance src_point = new_point;
            glm::vec3 p3 = glm::vec3(new_point.pos);

            // First apply the rigid transform
            glm::vec3 transformed = initial_R * p3 + source_translation;

            // Build a tangent basis from the target normal
            glm::vec3 helper = (std::abs(normal.y) < 0.9f)
                ? glm::vec3(0.0f, 1.0f, 0.0f)
                : glm::vec3(1.0f, 0.0f, 0.0f);

            glm::vec3 tangent1 = glm::normalize(glm::cross(helper, normal));
            glm::vec3 tangent2 = glm::normalize(glm::cross(normal, tangent1));

            // LiDAR-ish perturbation:
            // mostly tangential jitter, smaller normal jitter
            float t1 = tangent_noise_dist(rng);
            float t2 = tangent_noise_dist(rng);
            float n_off = normal_noise_dist(rng);

            glm::vec3 local_offset =
                tangent1 * t1 +
                tangent2 * t2 +
                normal * n_off;

            transformed += local_offset;

            src_point.pos = glm::vec4(transformed, 1.0f);
            src_point.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

            source_points.push_back(src_point);
            source_points_for_drawing.push_back(src_point);
        }
    }
    
    PointCloud target_point_cloud;
    PointCloud source_point_cloud;
    PointCloud source_point_cloud_for_drawing;
    target_point_cloud.update_points(target_points);
    source_point_cloud.update_points(source_points);
    

    glm::mat3 R = glm::mat3(1.0f);
    glm::vec3 t = glm::vec3(0.0f);
    

    // for (int i = 0; i < 10; i++) {
    //     icp_step(source_points, target_points, target_normals, R, t);
    // }

    // R * p + t;
    // for (size_t i = 0; i < source_points.size(); i++) {
    //     glm::vec3 p = R * glm::vec3(source_points[i].pos) + t;
    //     source_points_for_drawing[i].pos = glm::vec4(p, 1.0f);
    // }

    
    source_point_cloud_for_drawing.update_points(source_points_for_drawing);
        


    // std::vector<int> correspondences(source_points.size(), -1);

    // for (size_t i = 0; i < source_points.size(); i++) {
    //     size_t target_id = find_closest_id(target_points, source_points[i].pos);
    //     correspondences[i] = target_id;
    // }

    // glm::mat3 R = glm::mat3(1.0f);   // identity rotation
    // glm::vec3 t = glm::vec3(0.0f);   // zero translation

    // double H[6][6] = {};
    // double g[6] = {};

    // for (size_t i = 0; i < source_points.size(); i++) {
    //     // int target_id = correspondences[i];
    //     size_t target_id = find_closest_id(target_points, source_points[i].pos);

    //     glm::vec3 p = glm::vec3(source_points[i].pos);

    //     glm::vec3 x = R * p + t;
    //     glm::vec3 q = glm::vec3(target_points[target_id].pos);
    //     glm::vec3 n = glm::normalize(target_normals[target_id]);

    //     glm::vec3 a = glm::cross(x, n);
    //     double rhs = (double)glm::dot(n, q - x);

    //     double J[6] = {
    //         (double)a.x,
    //         (double)a.y,
    //         (double)a.z,
    //         (double)n.x,
    //         (double)n.y,
    //         (double)n.z
    //     };

    //     for (int r = 0; r < 6; r++) {
    //         g[r] += J[r] * rhs;

    //         for (int c = 0; c < 6; c++) {
    //             H[r][c] += J[r] * J[c];
    //         }
    //     }
    // }


    // // dumping
    // double lambda = 1e-6;
    // for (int i = 0; i < 6; i++) {
    //     H[i][i] += lambda;
    // }

    // double delta[6] = {};
    // bool ok = solve_6x6(H, g, delta);

    // glm::vec3 omega(
    //     (float)delta[0],
    //     (float)delta[1],
    //     (float)delta[2]
    // );

    // glm::vec3 v(
    //     (float)delta[3],
    //     (float)delta[4],
    //     (float)delta[5]
    // );

    // glm::mat3 dR = omega_to_mat3(omega);

    // R = dR * R;
    // t = dR * t + v;



    CirlceInstance circle_instance;
    // circle_instance.color = glm::vec4(1, 1, 1, 1);
    circle_instance.color = glm::vec4(0.8, 0.8, 0.2, 1);

    std::vector<CirlceInstance> instances;

    int row_size = 8;
    for (int x = 0; x < row_size; x++)
        for (int z = 0; z < row_size; z++) {
            circle_instance.position_radius.x = (x + 0.5) * 2;
            circle_instance.position_radius.z = (z + 0.5) * 2;

            circle_instance.color = glm::vec4((float)x / (float)row_size, 0, (float)z / (float)row_size, 1);

            instances.push_back(circle_instance);
        }
    
    CircleCloud circle_cloud = CircleCloud();
    circle_cloud.set_instances(instances);

    LightSource light_source;
    light_source.position = glm::vec4(-15, 2, 2, 8);
    light_source.color = glm::vec4(1, 0, 0, 1);
    engine.lighting_system.set_light_source(0, light_source);

    light_source.position = glm::vec4(-15, 2, -2, 8);
    light_source.color = glm::vec4(0, 1, 0, 1);
    engine.lighting_system.set_light_source(1, light_source);

    light_source.position = glm::vec4(-13, 2, 0, 8);
    light_source.color = glm::vec4(0, 0, 1, 1);
    engine.lighting_system.set_light_source(2, light_source);

    light_source.position = glm::vec4(-17, 2, 0, 8);
    light_source.color = glm::vec4(1, 1, 1, 1);
    engine.lighting_system.set_light_source(3, light_source);

    Sphere sphere = Sphere();
    sphere.mesh->position.x = -14;
    sphere.mesh->position.y = 1;

    PBRSkybox skybox = PBRSkybox(engine.texture_manager->st_peters_square_night_4k);
        
    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // window.draw(&skybox, &camera);

        skybox.attach_environment(*engine.default_program);
        skybox.attach_environment(*engine.default_circle_program);

        // sphere.mesh->position.x = -14 + cos(timer) * 4;
        // sphere.mesh->position.z = 0 + sin(timer) * 4;

        // voxel_grid->update(&window, &camera);
        // window.draw(voxel_grid, &camera);

        // window.draw(&point_cloud_mesh, &camera);

        window.draw(&target_point_cloud, &camera);
        window.draw(&source_point_cloud, &camera);
        // window.draw(&source_point_cloud_for_drawing, &camera);
        
        
        
        
        // window.draw(&circle_cloud, &camera);
        // window.draw(sphere.mesh, &camera);

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);

        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        if (ImGui::Button("ICP Step")) {
            icp_step(source_points, target_points, target_normals, R, t);

            for (size_t i = 0; i < source_points.size(); i++) {
                glm::vec3 p = R * glm::vec3(source_points[i].pos) + t;
                source_points[i].pos = glm::vec4(p, 1.0f);
            }

            R = glm::mat3(1.0f);
            t = glm::vec3(0.0f);

            source_point_cloud.update_points(source_points);
        }

        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
