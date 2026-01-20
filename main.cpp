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
#include "render_object_cube.h"
// #include "scene.h"
// #include "cube_prefab.h"


// class Grid : public Drawable, public Transformable {
// public:
//     Cube*** cubes;
//     int width;
//     int height;
    
//     Grid(int width, int height) {
//         this->width = width;
//         this->height = height;

//         cubes = new Cube**[width];

//         for (int x = 0; x < width; x++) {
//             cubes[x] = new Cube*[height];
//             for (int y = 0; y < height; y++) {
//                 cubes[x][y] = new Cube();
//                 cubes[x][y]->position.x = x * 2;
//                 cubes[x][y]->position.z = y * 2;
//             }
//         }
//     }

//     void draw(RenderState state) override {
//         state.transform *= get_model_matrix();

//         for (int x = 0; x < width; x++) {
//             for (int y = 0; y < height; y++) {
//                 cubes[x][y]->draw(state);
//             }
//         }
//     }
// };



class Grid : public Drawable, public Transformable {
public:
    RenderObjectCube*** cubes;
    int width;
    int height;
    std::vector<glm::mat4> instance_transforms;
    Mesh* cube_mesh;
    MaterialTemplate* cube_material_template;
    
    
    Grid(Engine3D* engine, int width, int height) {
        this->width = width;
        this->height = height;

        cubes = new RenderObjectCube**[width];
        

        for (int x = 0; x < width; x++) {
            cubes[x] = new RenderObjectCube*[height];
            for (int y = 0; y < height; y++) {
                cubes[x][y] = new RenderObjectCube(engine);
                cubes[x][y]->position.x = x * 2;
                cubes[x][y]->position.z = y * 2;

                instance_transforms.push_back(cubes[x][y]->get_model_matrix());
            }
        }
        cube_mesh = cubes[0][0]->mesh;
        cube_material_template = cubes[0][0]->material->templ;
    }

    void draw(RenderState state) {
        state.transform *= get_model_matrix();

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int id = x * width + y;
                instance_transforms[id] = state.transform * cubes[x][y]->get_model_matrix();
            }
        }

        cube_mesh->set_instance_transforms(instance_transforms);
        cube_mesh->draw_instanced(state, cube_material_template->program);
    }

    void draw_cubes_instanced() {

    }
};


int main() {
    Engine3D* engine = new Engine3D();
    Window* window = new Window(engine, 1280, 720, "3D visualization");
    engine->set_window(window);
    
    window->disable_cursor();

    Camera* camera = new Camera();
    window->set_camera(camera);

    FPSCameraController* camera_controller = new FPSCameraController(camera);

    // Scene* scene = new Scene(engine);
    // CubePrefab* cube_prefab = new CubePrefab();
    // SceneObject* cube = cube_prefab->instantiate(scene);
    

    // Cube* cube = new Cube();

    Grid* grid = new Grid(engine, 10, 10);
    float timer = 0;
    float lastFrame = 0;
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;

        camera_controller->update(window, delta_time);

        window->clear_color({0.776470588f, 0.988235294f, 1.0f, 1.0f});

        // for (int x = 0; x < grid->width; x++){
        //     for (int y = 0; y < grid->height; y++) {
        //         grid->cubes[x][y]->position.y = sin(((float)x / (float)grid->width) * 3.14 + timer * 4) + cos(((float)y / (float)grid->width) * 3.14 + timer * 4);
        //     }
        // }

        // window->draw(cube, camera);
        window->draw(grid, camera);


        // cube->position.x += 1.0 * delta_time;

        window->swap_buffers();
        engine->poll_events();
    }
}
