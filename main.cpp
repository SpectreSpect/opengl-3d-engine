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


    // glm::ivec3 min_pos = glm::ivec3(-30, -30, -30);
    // glm::ivec3 max_pos = glm::ivec3(30, 30, 30);

    AStar* a_star = new AStar(voxel_grid);

    // for (int x = min_pos.x; x <= max_pos.x; x++)
    //     for (int y = min_pos.y; y <= max_pos.y; y++)
    //         for (int z = min_pos.z; z <= max_pos.z; z++) {
    //             glm::ivec3 pos = glm::ivec3(x, y, z);
    //             uint64_t key = VoxelGrid::pack_key(x, y, z);
                
    //             Voxel voxel = voxel_grid->get_voxel(pos);

    //             OccupancyCell* occupancy_cell = new OccupancyCell(0, voxel.visible);
                
    //             a_star->grid->set_cell(pos, occupancy_cell);
    //         }
            
    glm::ivec3 start_pos = glm::ivec3(1, 13, 0);
    glm::ivec3 end_pos = glm::ivec3(20, 16, 0);

    // std::vector<glm::ivec3> path = a_star->find_path(start_pos, end_pos);
    std::vector<glm::ivec3> path = std::vector<glm::ivec3>();

    // std::cout << path.size() << std::endl;

    // path = a_star->find_path(start_pos, end_pos);

    // voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
        
    //     Voxel start_voxel = Voxel();
    //     start_voxel.color = glm::vec3(1.0, 0, 0);
    //     start_voxel.visible = true;

    //     voxel_editor.set(start_pos, start_voxel);


    //     Voxel end_voxel = Voxel();
    //     end_voxel.color = glm::vec3(0.0, 0.0, 1.0);
    //     end_voxel.visible = false;

    //     voxel_editor.set(end_pos, end_voxel);


    //     for (int i = 0; i < path.size() - 2; i++) {
    //         glm::ivec3 pos = path[i];

    //         // std::cout << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
            
    //         float value = (float)i / path.size();

    //         Voxel voxel = Voxel();
    //         voxel.color = {0, value, 0};
    //         voxel.visible = true;

    //         voxel_editor.set(pos, voxel);
    //     }
    // });

    // a_star->grid->occupancy_cells[]

    Line* line = new Line();

    LineInstance line_instance1;
    line_instance1.p0 = glm::vec3(0, 20, 0);
    line_instance1.p1 = glm::vec3(0, 25, 0);

    LineInstance line_instance2;
    line_instance2.p0 = glm::vec3(0, 25, 0);
    line_instance2.p1 = glm::vec3(5, 25, 0);

    std::vector<LineInstance> line_instances;
    line_instances.push_back(line_instance1);
    line_instances.push_back(line_instance2);

    line->set_lines(line_instances);

    // line->set_lines(line_instance);

    glm::vec3 prev_cam_pos = camera_controller->camera->position;
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

        // ImGui::Begin("A star");

        // if (ImGui::Button("Set start")) {
        if (glfwGetKey(window->window, GLFW_KEY_R) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel start_voxel = Voxel();
                start_voxel.color = glm::vec3(1.0, 0.0, 0.0);
                start_voxel.visible = false;

                voxel_editor.set(start_pos, start_voxel);

                start_pos = voxel_pos;

                start_voxel.visible = true;

                voxel_editor.set(start_pos, start_voxel);
            });
        }
        // }

        if (glfwGetKey(window->window, GLFW_KEY_F) == GLFW_PRESS) {
            glm::ivec3 voxel_pos = glm::ivec3(glm::floor(camera->position));

            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel end_voxel = Voxel();
                end_voxel.color = glm::vec3(0.0, 0.0, 1.0);
                end_voxel.visible = false;

                voxel_editor.set(end_pos, end_voxel);

                end_pos = voxel_pos;

                end_voxel.visible = true;

                voxel_editor.set(end_pos, end_voxel);
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

            // std::cout << "(" << start_pos.x << ", " << start_pos.y << ", " << start_pos.z << ")" << std::endl;
            // std::cout << "(" << end_pos.x << ", " << end_pos.y << ", " << end_pos.z << ")" << std::endl;


            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
                Voxel start_voxel = Voxel();
                start_voxel.color = glm::vec3(1.0, 0.0, 0.0);
                start_voxel.visible = false;

                // std::cout << "(" << start_pos.x << ", " << start_pos.y << ", " << start_pos.z << ")" << std::endl;
                voxel_editor.set(start_pos, start_voxel);
                voxel_editor.set(end_pos, start_voxel);

                // std::cout << path.size() - 2 << std::endl;
                if (path.size() > 0)
                for (int i = 0; i < path.size() - 2; i++) {
                    // std::cout << i << std::endl;
                    glm::ivec3 pos = path[i];

                    // // std::cout << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                    
                    float value = (float)i / path.size();

                    Voxel voxel = Voxel();
                    voxel.color = {1.0f, 1.0f, 1.0f};
                    voxel.visible = false;

                    voxel_editor.set(pos, voxel);
                }


            });
            
            std::vector<glm::ivec3> new_path = a_star->find_path(start_pos, end_pos);


            voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

                path = new_path;

                if (path.size() > 0)
                for (int i = 0; i < path.size() - 2; i++) {
                    glm::ivec3 pos = path[i];

                    // std::cout << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                    
                    float value = (float)i / path.size();

                    Voxel voxel = Voxel();
                    voxel.color = {0, value, 0};
                    voxel.visible = true;

                    voxel_editor.set(pos, voxel);
                }

                Voxel start_voxel = Voxel();
                start_voxel.color = glm::vec3(1.0, 0.0, 0.0);
                start_voxel.visible = true;

                voxel_editor.set(start_pos, start_voxel);
                start_voxel.color = glm::vec3(0.0, 0.0, 1.0);
                voxel_editor.set(end_pos, start_voxel);
            });



            // voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){
            //     Voxel end_voxel = Voxel();
            //     end_voxel.color = glm::vec3(0.0, 0.0, 1.0);
            //     end_voxel.visible = false;

            //     voxel_editor.set(end_pos, end_voxel);

            //     end_pos = voxel_pos;

            //     end_voxel.visible = true;

            //     voxel_editor.set(end_pos, end_voxel);
            // });
        }
        // if (ImGui::Button("Set end")) {
            
        // }

        // ImGui::End();


        // ImGui::Begin("Debug");

        // auto on_change_triangle = [&](glm::vec3& p0, glm::vec3& p1, glm::vec3& p2) {
        //     auto key = voxel_grid->pack_key(triangle_chunk_pos.x, triangle_chunk_pos.y, triangle_chunk_pos.z);

        //     auto it = voxel_grid->chunks.find(key);
        //     if (it == voxel_grid->chunks.end()) return 1;

        //     Chunk* chunk = it->second;
        //     voxel_rastorizator->gridable = chunk;

        //     chunk->clear_voxels();

        //     // triangle rasterization
        //     auto voxel_generator = [&](glm::ivec3 point) {
        //         glm::vec3 P = (glm::vec3(point) + 0.5f) * voxel_size;

        //         glm::vec3 color = glm::vec3(1.0f);
        //         glm::vec3 w = math_utils::bary_coords(p0, p1, p2, P);
        //         if (w.x >= 0.0f) {
        //             w = glm::clamp(w, glm::vec3(0.0f), glm::vec3(1.0f));
        //             float s = w.x + w.y + w.z;
        //             if (s > 0.0f) w /= s;
        //             color = c0 * w.x + c1 * w.y + c2 * w.z;
        //         }

        //         Voxel v{};
        //         v.visible = true;
        //         v.color = color;
        //         return v;
        //     };

        //     voxel_rastorizator->rasterize_triangle(p0, p1, p2, voxel_size, voxel_generator);
        //     triangle->update_vbo(p0+chunk_origin, p1+chunk_origin, p2+chunk_origin, c0, c1, c2);

        //     // update chunk mesh
        //     voxel_grid->chunks_to_update.insert(key);
        // };

        // triangle_controller->triangle_controller_element(p0, p1, p2, c0, c1, c2, on_change_triangle);

        // if (ImGui::Button("Rasterize the triangle")) {

        // }


        // ImGui::End();

        ui::end_frame();

        // line->draw_instanced(1);
        window->draw(line, camera);

        window->swap_buffers();
        engine->poll_events();

        prev_cam_pos = camera_controller->camera->position;
    }
    
    ui::shutdown();
}
