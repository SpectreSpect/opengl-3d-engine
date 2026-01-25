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
    LineInstance middle_line;
    middle_line.p0 = p0;
    middle_line.p1 = p1;

    float t_pos = 0.9;
    glm::vec3 tip_to_back_dir = glm::normalize(p0 - p1);
    glm::vec3 arrow_tip_start_pos = p1 + tip_to_back_dir * t_pos;
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 tip_dir = glm::normalize(glm::cross(arrow_tip_start_pos, up));
    
    float tip_width = 0.5f;

    LineInstance left_line;
    left_line.p0 = arrow_tip_start_pos + tip_dir * tip_width;
    // NonholonomicAStar::print_vec();
    left_line.p1 = p1;

    LineInstance right_line;
    right_line.p0 = arrow_tip_start_pos + tip_dir * -tip_width;
    right_line.p1 = p1;

    // std::vector<LineInstance> result = {left_line, middle_line, right_line};
    std::vector<LineInstance> result = {left_line, middle_line, right_line};

    return result;
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
    // push_back(line_instances, get_arrow({0, 20, 0}, {20, 20, 0}));
    // push_back(line_instances, get_arrow({20, 20, 0}, {30, 20, 7}));


    Line* path_arrows = new Line();
    // test_line->set_lines(get_arrow(start_pos.pos, end_pos.pos));
    // test_line->set_lines(get_arrow({0, 20, 0}, {20, 20, 0}));
    // test_line->set_lines(line_instances);
    // test_line->color = {0.0f, 0.0f, 1.0f};
    path_arrows->width = 5.0f;
    path_arrows->color = {0.619607843f, 0.345098039, 0.37254902};


    NonholonomicAStar* nonholonomic_astar = new NonholonomicAStar(voxel_grid);
    NonholonomicPos npos;
    npos.pos.y = 20;
    std::vector<Line*> lines;

    for (int dir = -1; dir <= 1; dir += 2)
        for (int steer = -1; steer <= 1; steer++) {
            std::vector<NonholonomicPos> motion = nonholonomic_astar->simulate_motion(npos, steer, dir);
            
            std::vector<LineInstance> line_instances;
            for (int i = 0; i < motion.size() - 1; i++) {
                LineInstance line_instance;
                line_instance.p0 = motion[i].pos;
                line_instance.p1 = motion[i + 1].pos;

                // std::cout << "(" << line_instance.p0.x << ", " << line_instance.p0.y << ", " << line_instance.p0.z << ")" << std::endl;
                // std::cout << "(" << npos.pos.x << ", " << npos.pos.y << ", " << npos.pos.z << ")" << std::endl;
                
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
                Voxel start_voxel = Voxel();
                start_voxel.color = glm::vec3(1.0, 0.0, 0.0);
                start_voxel.visible = false;

                voxel_editor.set(start_pos.pos, start_voxel);

                start_pos.pos = voxel_pos;

                start_voxel.visible = true;

                voxel_editor.set(start_pos.pos, start_voxel);
            });
        }
        // }

        if (glfwGetKey(window->window, GLFW_KEY_F) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel end_voxel = Voxel();
                end_voxel.color = glm::vec3(0.0, 0.0, 1.0);
                end_voxel.visible = false;

                voxel_editor.set(end_pos.pos, end_voxel);

                end_pos.pos = voxel_pos;

                end_voxel.visible = true;

                voxel_editor.set(end_pos.pos, end_voxel);
            });
        }

        if (glfwGetKey(window->window, GLFW_KEY_V) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel new_voxel = Voxel();
                new_voxel.color = glm::vec3(1.0, 1.0, 1.0);
                new_voxel.visible = true;

                voxel_editor.set(voxel_pos, new_voxel);
            });
        }


        if (glfwGetKey(window->window, GLFW_KEY_T) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));


            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel start_voxel = Voxel();
                start_voxel.color = glm::vec3(1.0, 0.0, 0.0);
                start_voxel.visible = false;

                voxel_editor.set(start_pos.pos, start_voxel);
                voxel_editor.set(end_pos.pos, start_voxel);

                if (path.size() > 0)
                for (int i = 0; i < path.size() - 1; i++) {
                    NonholonomicPos pos = path[i];


                    push_back(arrow_line_instances, get_arrow(path[i].pos, path[i+1].pos));

                    
                    // float value = (float)i / path.size();

                    // Voxel voxel = Voxel();
                    // voxel.color = {1.0f, 1.0f, 1.0f};
                    // voxel.visible = false;

                    // voxel_editor.set(pos.pos, voxel);
                }
                path_arrows->set_lines(arrow_line_instances);


            });

            std::vector<NonholonomicPos> new_path = nonholonomic_astar->find_nonholomic_path(start_pos, end_pos);


            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

                path = new_path;

                if (path.size() > 0)
                for (int i = 0; i < path.size() - 2; i++) {
                    NonholonomicPos pos = path[i];
                    
                    float value = (float)i / path.size();

                    Voxel voxel = Voxel();
                    voxel.color = glm::vec3(0.796078431, 0.023529412, 0.749019608) * ((rand() % 255) / 255.0f);
                    voxel.visible = true;

                    voxel_editor.set(pos.pos, voxel);
                }

                Voxel start_voxel = Voxel();
                start_voxel.color = glm::vec3(1.0, 0.0, 0.0);
                start_voxel.visible = true;

                voxel_editor.set(start_pos.pos, start_voxel);
                start_voxel.color = glm::vec3(0.0, 0.0, 1.0);
                voxel_editor.set(end_pos.pos, start_voxel);
            });
        }
        ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_Always);
        ImGui::Begin("Debug");

        glm::vec3 p = camera->position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        ImGui::End();

        ui::end_frame();

        for (int i = 0; i < lines.size(); i++)
            window->draw(lines[i], camera);
        
        window->draw(path_arrows, camera);
        
        window->swap_buffers();
        engine->poll_events();

    }
    
    ui::shutdown();
}
