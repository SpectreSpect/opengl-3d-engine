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


class Grid : public Drawable, public Transformable {
public:
    Cube*** cubes;
    int width;
    int height;
    
    Grid(int width, int height) {
        this->width = width;
        this->height = height;

        cubes = new Cube**[width];

        for (int x = 0; x < width; x++) {
            cubes[x] = new Cube*[height];
            for (int y = 0; y < height; y++) {
                cubes[x][y] = new Cube();
                cubes[x][y]->position.x = x * 2;
                cubes[x][y]->position.z = y * 2;
            }
        }
    }

    void draw(RenderState state) override {
        state.transform *= get_model_matrix();

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                cubes[x][y]->draw(state);
            }
        }
    }
};

// rgba(219, 188, 45)
float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
// float clear_col[4] = {0.858823529, 0.737254902, 0.176470588, 1.0f}; // Venus
// float clear_col[4] = {0.952941176, 0.164705882, 0.054901961, 1.0f};

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


template<class T>
void push_back(std::vector<T>& a, const std::vector<T>& b) {
    a.insert(a.end(), b.begin(), b.end());
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
    sleep(1);
    // VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, {12, 12, 12});

    float voxel_size = 1.0f;
    float chunk_render_size = voxel_grid->chunk_size.x * voxel_size;
    TriangleController* triangle_controller = new TriangleController(chunk_render_size, chunk_render_size);
    
    glm::vec3 p0(0, chunk_render_size, 0.0f);
    glm::vec3 p1(chunk_render_size, chunk_render_size, 0.0f);
    glm::vec3 p2(chunk_render_size / 2.0f, 0.0f, chunk_render_size);

    glm::vec3 c0(1.0f, 0.0f, 0.0f);
    glm::vec3 c1(0.0f, 1.0f, 0.0f);
    glm::vec3 c2(0.0f, 0.0f, 1.0f);

    glm::ivec3 triangle_chunk_pos = glm::ivec3(0, 2, 0);
    glm::vec3 chunk_origin = glm::vec3(triangle_chunk_pos) * glm::vec3(voxel_grid->chunk_size) * voxel_size;

    Triangle* triangle = new Triangle(p0+chunk_origin, p1+chunk_origin, p2+chunk_origin, c0, c1, c2);

    VoxelRastorizator* voxel_rastorizator = new VoxelRastorizator(nullptr);


    AStar* a_star = new AStar(voxel_grid);


    NonholonomicPos start_pos = NonholonomicPos{glm::ivec3(1, 13, 0), 0};
    NonholonomicPos end_pos = NonholonomicPos{glm::ivec3(20, 16, 0), 0};

    std::vector<NonholonomicPos> path = std::vector<NonholonomicPos>();

    std::vector<LineInstance> arrow_line_instances;
    std::vector<Line*> path_lines;
    std::vector<std::vector<LineInstance>> path_line_instances;

    Line* start_dir_line = new Line();
    start_dir_line->color = glm::vec3(1.0f, 0.501960784, 0);
    Line* end_dir_line = new Line();
    end_dir_line->color = glm::vec3(0.023529412f, 0.768627451f, 1.0f);


    Line* path_arrows = new Line();
    path_arrows->width = 5.0f;
    path_arrows->color = {1.0f, 0.0f, 0.0f};


    NonholonomicAStar* nonholonomic_astar = new NonholonomicAStar(voxel_grid);
    nonholonomic_astar->use_reed_shepps_fallback = false;
    NonholonomicPos npos;
    npos.pos.y = 20;
    std::vector<Line*> lines;

    std::vector<NonholonomicPos> reeds_shepp_path = nonholonomic_astar->find_reeds_shepp(start_pos, end_pos);

    bool simulation_running = false;

    std::vector<Line> closed_heap_lines;


    for (int dir = -1; dir <= 1; dir += 2)
        for (int steer = -1; steer <= 1; steer++) {
            std::vector<NonholonomicPos> motion = nonholonomic_astar->simulate_motion(npos, steer, dir);
            
            std::vector<LineInstance> line_instances;
            for (int i = 0; i < motion.size() - 1; i++) {
                LineInstance line_instance;
                line_instance.p0 = motion[i].pos;
                line_instance.p1 = motion[i + 1].pos;


                line_instances.push_back(line_instance);
            }

            Line* line = new Line();
            line->set_lines(line_instances);

            lines.push_back(line);
        }
    
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
        window->draw(triangle, camera);


        if (glfwGetKey(window->window, GLFW_KEY_R) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

                start_pos.pos = camera->position;
                start_pos.theta = glm::radians(camera_controller->yaw);

                float angle = start_pos.theta; // or + 3.14159265f
                glm::vec3 dir(std::cos(start_pos.theta), 0.0f, std::sin(start_pos.theta));
                start_dir_line->set_lines(get_arrow(start_pos.pos, start_pos.pos + dir * 1.0f));
            });
        }
        // }

        if (glfwGetKey(window->window, GLFW_KEY_F) == GLFW_PRESS) {
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
        if (glfwGetKey(window->window, GLFW_KEY_I) == GLFW_PRESS) {
            nonholonomic_astar->initialize(start_pos, end_pos);
            simulation_running = false;
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

            std::vector<NonholonomicPos> path = nonholonomic_astar->find_nonholomic_path(start_pos, end_pos);

            nonholonomic_astar->adjust_and_check_path(path);

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
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Always);
        ImGui::Begin("Debug");

        glm::vec3 p = camera->position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        
        ImGui::SliderFloat("Change steer pentalty", &nonholonomic_astar->change_steer_pentalty, 0, 64);
        ImGui::SliderFloat("Switch dir pentalty", &nonholonomic_astar->switch_dir_pentalty, 0, 64);

        ImGui::End();

        ui::end_frame();

        for (int i = 0; i < lines.size(); i++)
            window->draw(lines[i], camera);
        
        // window->draw(path_arrows, camera);

        for (int i = 0; i < path_lines.size(); i++)
            window->draw(path_lines[i], camera);
        
        // std::cout << nonholonomic_astar->state_lines.size() << std::endl;
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
