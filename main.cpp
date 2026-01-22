// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

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

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

int main() {
    Engine3D* engine = new Engine3D();
    Window* window = new Window(engine, 1280, 720, "3D visualization");
    engine->set_window(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window->window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    window->disable_cursor();

    Camera* camera = new Camera();
    window->set_camera(camera);

    FPSCameraController* camera_controller = new FPSCameraController(camera);
    camera_controller->speed = 20;

    float timer = 0;
    float lastFrame = 0;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, {24, 6, 24});

    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        camera_controller->update(window, delta_time);

        window->clear_color({0.776470588f, 0.988235294f, 1.0f, 1.0f});

        voxel_grid->update(camera);
        window->draw(voxel_grid, camera);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow(); // проверка

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        window->swap_buffers();
        engine->poll_events();
    }
}
