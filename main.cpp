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
#include "vtk_mesh_loader.h"
#include "path_utils.h"

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

float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};

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
    // VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, {12, 12, 12});

    int vertex_stride = 9;
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

    Cube* cube = new Cube();
    cube->position = glm::vec3(0.0f, chunk_render_size * 5, 0.0f);
    cube->scale = glm::vec3(10.0f);
    cube->set_color(glm::vec3(1.0f, 1.0f, 1.0f));

    Triangle* triangle = new Triangle(p0+chunk_origin, p1+chunk_origin, p2+chunk_origin, c0, c1, c2);

    VoxelRastorizator* voxel_rastorizator = new VoxelRastorizator(voxel_grid);
    VtkMeshLoader* vtk_mesh_loader = new VtkMeshLoader(*cube->mesh->vertex_layout);

    MeshData model_mesh_data = vtk_mesh_loader->load_mesh((executable_dir() / "models" / "test_mesh.vtk").string());

    Mesh* model = new Mesh(model_mesh_data.vertices, model_mesh_data.indices, cube->mesh->vertex_layout); 
    model->position = {0.0f, chunk_render_size * 5, 0.0f};
    model->scale = glm::vec3(5.0f);

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
        // window->draw(cube, camera);
        window->draw(model, camera);

        ImGui::Begin("Debug");

        ImGui::TextUnformatted("Camera position");

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255));
        ImGui::Text("x: %.3f", camera_controller->camera->position.x);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 120, 255));
        ImGui::Text("y: %.3f", camera_controller->camera->position.y);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 180, 255, 255));
        ImGui::Text("z: %.3f", camera_controller->camera->position.z);
        ImGui::PopStyleColor();

        if (ImGui::Button("Rasterize the triangle")) {
            // MeshData mesh_data = cube->create_mesh_data(cube->get_color());

            auto voxel_generator = [&](glm::vec3 point) -> Voxel {
                Voxel voxel;
                voxel.color = glm::vec3(1.0f, 0.0f, 0.0f);
                voxel.visible = rand() % 10000 > 5000;
                return voxel;
            };

            voxel_rastorizator->rasterize_mesh(
                model_mesh_data, 
                model->get_model_matrix(), 
                voxel_generator, 
                voxel_size, 
                0, 
                vertex_stride
            );
        }


        ImGui::End();

        ui::end_frame();

        window->swap_buffers();
        engine->poll_events();

        prev_cam_pos = camera_controller->camera->position;
    }
    
    ui::shutdown();
}
