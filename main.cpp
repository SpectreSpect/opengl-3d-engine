// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
#include "math_utils.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"


float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

std::vector<LineInstance> get_arrow(glm::vec3 p0, glm::vec3 p1) {
    LineInstance middle{p0, p1};

    glm::vec3 d = p1 - p0;
    float len = glm::length(d);
    if (len < 1e-6f) return { middle };

    glm::vec3 dir = d / len; // arrow direction (unit)

    // --- constant arrowhead size (world units) ---
    const float head_len   = 0.3f; // distance from tip back to where head starts
    const float head_width = 0.2f; // half-width of the V

    // Put head base head_len behind the tip, but don't go past p0 for very short arrows
    float back = std::min(head_len, 0.9f * len);
    glm::vec3 head_base = p1 - dir * back;

    // Choose a reference axis not parallel to dir
    glm::vec3 ref(0, 1, 0);
    if (std::abs(glm::dot(dir, ref)) > 0.99f)
        ref = glm::vec3(1, 0, 0);

    glm::vec3 side = glm::normalize(glm::cross(dir, ref));

    LineInstance left;
    left.p0 = head_base + side * head_width;
    left.p1 = p1;

    LineInstance right;
    right.p0 = head_base - side * head_width;
    right.p1 = p1;

    // return { left, middle, right };
    return { left, middle, right };
}

void draw_f(Window* window, 
    Camera* camera, 
    VoxelGrid* voxel_grid, 
    NonholonomicAStar* astar,
    PlainAstarData plain_a_star_path,
    const NonholonomicPos& start_pos, const NonholonomicPos& end_pos, 
    int size) {
    glm::ivec3 camera_voxel_pos = glm::ivec3(glm::floor(camera->position));

    voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

        glm::ivec3 left_top_pos = (glm::ivec3)start_pos.pos -  glm::ivec3(size/2.0f, 0, size/2.0f);

        Voxel paint_voxel;

        float max_f = 0;
        for (int x = 0; x < size; x++)
            for (int z = 0; z < size; z++) {
                NonholonomicPos new_pos;
                new_pos.pos = (glm::vec3)left_top_pos + glm::vec3(x, 0, z);

                float f = astar->get_nonholonomic_f(new_pos, end_pos, new_pos, plain_a_star_path);

                if (f > max_f)
                    max_f = f;
            }

        for (int x = 0; x < size; x++)
            for (int z = 0; z < size; z++) {
                NonholonomicPos new_pos;
                new_pos.pos = (glm::vec3)left_top_pos + glm::vec3(x, 0, z);

                float f = astar->get_nonholonomic_f(new_pos, end_pos, new_pos, plain_a_star_path);

                float color_value = f / max_f;

                paint_voxel.color = glm::vec3(0.0, 0.0, color_value);
                paint_voxel.visible = true;

                voxel_editor.set((glm::ivec3)new_pos.pos - glm::ivec3(0, 1, 0), paint_voxel);
            }
    });
}


int main() {
    Engine3D* engine = new Engine3D();
    Window* window = new Window(engine, 1280, 720, "3D visualization");
    engine->set_window(window);
    ui::init(window->window);
    
    window->disable_cursor();

    Camera* camera = new Camera();
    window->set_camera(camera);

    FPSCameraController* camera_controller = new FPSCameraController(camera);
    camera_controller->speed = 20;

    float timer = 0;
    float lastFrame = 0;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, {24, 6, 24});
    voxel_grid->update(window, camera);
    // sleep(1);

    NonholonomicPos start_pos = NonholonomicPos{glm::ivec3(1, 13, 0), 0};
    NonholonomicPos end_pos = NonholonomicPos{glm::ivec3(20, 16, 0), 0};

    std::vector<Line*> path_lines;
    std::vector<std::vector<LineInstance>> path_line_instances;

    Line* start_dir_line = new Line();
    start_dir_line->color = glm::vec3(1.0f, 0.501960784, 0);
    Line* end_dir_line = new Line();
    end_dir_line->color = glm::vec3(0.023529412f, 0.768627451f, 1.0f);

    NonholonomicAStar* nonholonomic_astar = new NonholonomicAStar(voxel_grid);
    bool simulation_running = false;

    bool TEMPPPPPPPPPPPPPPPPp = true;
    
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(window);

        camera_controller->update(window, delta_time);

        window->clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        voxel_grid->update(window, camera);
        window->draw(voxel_grid, camera);

        if (glfwGetKey(window->window, GLFW_KEY_R) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

                start_pos.pos = camera->position;
                start_pos.theta = glm::radians(camera_controller->yaw);

                float angle = start_pos.theta;
                glm::vec3 dir(std::cos(start_pos.theta), 0.0f, std::sin(start_pos.theta));
                start_dir_line->set_lines(get_arrow(start_pos.pos, start_pos.pos + dir * 1.0f));
            });
        }

        if (glfwGetKey(window->window, GLFW_KEY_F) == GLFW_PRESS) {
            TEMPPPPPPPPPPPPPPPPp = true;
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

                end_pos.pos = camera->position;
                end_pos.theta = glm::radians(camera_controller->yaw);

                float angle = end_pos.theta; // or + 3.14159265f
                glm::vec3 dir(std::cos(end_pos.theta), 0.0f, std::sin(end_pos.theta));
                end_dir_line->set_lines(get_arrow(end_pos.pos, end_pos.pos + dir * 1.0f));
            });
        }

        if (glfwGetKey(window->window, GLFW_KEY_V) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel new_voxel = Voxel();

                for (int i = 0; i < 10; i++) {
                    new_voxel.color = glm::vec3(1.0, 1.0, 1.0);
                    new_voxel.visible = true;
                    
                    voxel_editor.set(voxel_pos, new_voxel);
                    voxel_pos.y += 1;
                }
            });
        }

        if (glfwGetKey(window->window, GLFW_KEY_Q) == GLFW_PRESS) {   
            glm::vec3 camera_dir = glm::normalize(camera->front);
            glm::vec3 start_pos = camera->position;
            glm::vec3 end_pos = camera->position + camera_dir * 100.0f;
            
            std::vector<glm::ivec3> intersected_voxel_poses = voxel_grid->line_intersects(start_pos, end_pos);
            // voxel_grid->adjust_to_ground(intersected_voxel_poses);

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){          
                Voxel new_voxel = Voxel();

                for (int i = 0; i < intersected_voxel_poses.size(); i++) {
                    new_voxel.color = glm::vec3(1.0, 1.0, 1.0);
                    new_voxel.visible = true;
                    
                    voxel_editor.set(intersected_voxel_poses[i], new_voxel);
                }
            });
        }

        if (glfwGetKey(window->window, GLFW_KEY_E) == GLFW_PRESS && TEMPPPPPPPPPPPPPPPPp) {  
            TEMPPPPPPPPPPPPPPPPp = false; 
            // glm::vec3 camera_dir = glm::normalize(camera->front);
            // glm::vec3 start_pos = camera->position;
            // glm::vec3 end_pos = camera->position + camera_dir * 100.0f;

            std::vector<glm::ivec3> output;
            std::vector<glm::vec3> polyline;

            polyline.push_back(start_pos.pos);
            polyline.push_back(camera->position);
            polyline.push_back(end_pos.pos);

            voxel_grid->get_ground_positions(polyline, output);

            
            // std::vector<glm::ivec3> intersected_voxel_poses = voxel_grid->line_intersects(start_pos, end_pos);
            // voxel_grid->adjust_to_ground(intersected_voxel_poses);

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){          
                Voxel new_voxel = Voxel();

                for (int i = 0; i < output.size(); i++) {
                    new_voxel.color = glm::vec3(1.0, 1.0, 1.0);
                    new_voxel.visible = true;
                    
                    voxel_editor.set(output[i], new_voxel);
                }
            });
        }
        // std::cout << nonholonomic_astar->state_pq.size() << std::endl;

        if (glfwGetKey(window->window, GLFW_KEY_I) == GLFW_PRESS) {            
            // voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor) {

            //     for (int i = 0; i < nonholonomic_astar->state_plain_astar_path.path.size(); i++) {
            //         // nonholonomic_astar->state_plain_astar_path[i]
            //         glm::ivec3 pos = nonholonomic_astar->state_plain_astar_path.path[i];

            //         Voxel voxel;
            //         voxel.color = {0.2f, 0.2f, 0.2f};
            //         voxel.visible = true;

            //         // std::cout << voxel.curvature << std::endl;

            //         voxel_editor.set(pos + glm::ivec3(0, -1, 0), voxel);
            //     }
            // });

            nonholonomic_astar->initialize(start_pos, end_pos);
            simulation_running = false;

            // draw_f(window, camera, voxel_grid, nonholonomic_astar, nonholonomic_astar->state_plain_astar_path, start_pos, end_pos, 200);

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor) {
                for (int i = 0; i < nonholonomic_astar->state_plain_astar_path.path.size(); i++) {
                    // nonholonomic_astar->state_plain_astar_path[i]
                    glm::ivec3 pos = nonholonomic_astar->state_plain_astar_path.path[i];
                    glm::ivec3 ground_voxel_pos = pos + glm::ivec3(0, -1, 0);

                    Voxel old_voxel = voxel_editor.get(ground_voxel_pos);

                    Voxel voxel;
                    voxel.color = {0.0, 1.0, 0.0};
                    voxel.visible = true;
                    voxel.curvature = old_voxel.curvature;

                    voxel_editor.set(ground_voxel_pos, voxel);
                }
            });
        }


        if (glfwGetKey(window->window, GLFW_KEY_O) == GLFW_PRESS) {
            simulation_running = true;
        }

        if (simulation_running) {
            if (nonholonomic_astar->find_nonholomic_path_step()) {
                simulation_running = false;
                std::cout << "Simulation ended" << std::endl;
            }
        }
        // std::cout << "---" << simulation_running << std::endl;


        if (glfwGetKey(window->window, GLFW_KEY_T) == GLFW_PRESS) {

            // std::vector<NonholonomicPos> path = nonholonomic_astar->find_nonholomic_path(start_pos, end_pos);
            std::vector<NonholonomicPos>& path = nonholonomic_astar->state_path;

            // nonholonomic_astar->adjust_and_check_path(path);

            path_lines.clear();

            std::vector<LineInstance> cur;
            cur.reserve(256);

            auto flush = [&](int dir) {
                if (cur.empty()) return;
                Line* line = new Line();
                line->color = (dir == 1) ? glm::vec3(1,0,0) : glm::vec3(0,0,1); // red forward, blue reverse
                line->set_lines(cur);
                path_lines.push_back(line);
                cur.clear();
            };

            if (path.size() > 1) {
                int cur_dir = path[1].dir; // dir for the first segment (0 -> 1)

                for (int i = 0; i < (int)path.size() - 1; ++i) {
                    int seg_dir = path[i + 1].dir; // direction of motion from i to i+1

                    if (seg_dir != cur_dir) {
                        flush(cur_dir);     // flush previous group with its own color
                        cur_dir = seg_dir;  // start new group
                    }

                    glm::vec3 a = path[i].pos   + glm::vec3(0, 0.2f, 0);
                    glm::vec3 b = path[i+1].pos + glm::vec3(0, 0.2f, 0);

                    cur.push_back(LineInstance{a, b});
                }

                flush(cur_dir);
            }
        }

        // ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
        ImGui::Begin("Debug");

        glm::vec3 p = camera->position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        ImGui::End();

        ui::end_frame();

        for (int i = 0; i < path_lines.size(); i++)
            window->draw(path_lines[i], camera);
        
        for (int i = 0; i < nonholonomic_astar->state_lines.size(); i++) {
            window->draw(nonholonomic_astar->state_lines[i], camera);
        }
            
        window->draw(start_dir_line, camera);
        window->draw(end_dir_line, camera);
        
        window->swap_buffers();
        engine->poll_events();
    }
    
    ui::shutdown();
}
