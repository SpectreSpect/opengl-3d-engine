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
// #include "scene.h"
// #include "cube_prefab.h"


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

// constexpr int BITS = 21;
// constexpr uint64_t MASK = (1ull << BITS) - 1; // 0x1FFFFF
// constexpr int OFFSET = (1 << (BITS-1)); // offset to encode signed -> unsigned

// uint64_t packKey(int32_t cx, int32_t cy, int32_t cz) {
//     if (cx < -OFFSET || cx > OFFSET - 1 ||
//         cy < -OFFSET || cy > OFFSET - 1 ||
//         cz < -OFFSET || cz > OFFSET - 1) {
//         throw std::out_of_range("chunk coord out of packable range");
//     }
//     uint64_t ux = (uint64_t)( (uint32_t)(cx + OFFSET) & (uint32_t)MASK );
//     uint64_t uy = (uint64_t)( (uint32_t)(cy + OFFSET) & (uint32_t)MASK );
//     uint64_t uz = (uint64_t)( (uint32_t)(cz + OFFSET) & (uint32_t)MASK );
//     return (ux << (BITS*2)) | (uy << BITS) | uz;
// }

// void unpackKey(uint64_t key, int32_t &cx, int32_t &cy, int32_t &cz) {
//     uint64_t ux = (key >> (BITS*2)) & MASK;
//     uint64_t uy = (key >> BITS) & MASK;
//     uint64_t uz = key & MASK;
//     cx = (int)( (int)ux - OFFSET );
//     cy = (int)( (int)uy - OFFSET );
//     cz = (int)( (int)uz - OFFSET );
// }


int main() {




    Engine3D* engine = new Engine3D();
    Window* window = new Window(engine, 1280, 720, "3D visualization");
    engine->set_window(window);
    
    window->disable_cursor();

    Camera* camera = new Camera();
    window->set_camera(camera);

    FPSCameraController* camera_controller = new FPSCameraController(camera);
    camera_controller->speed = 20;

    // Scene* scene = new Scene(engine);
    // CubePrefab* cube_prefab = new CubePrefab();
    // SceneObject* cube = cube_prefab->instantiate(scene);
    

    // Cube* cube = new Cube();

    // Chunk* chunk = new Chunk({16, 16, 16}, {1, 1, 1});
    // chunk->scale = {0.1, 0.1, 0.1};

    Chunk* big_chunk = new Chunk({16, 16, 16}, {1, 1, 1});
    big_chunk->scale = {2, 2, 2};

    Chunk* small_chunk = new Chunk({16, 16, 16}, {1, 1, 1});
    small_chunk->scale = {0.1, 0.1, 0.1};

    Grid* grid = new Grid(10, 10);
    float timer = 0;
    float lastFrame = 0;


    // std::unordered_map<uint64_t, Chunk*> chunks;

    

    // chunks[packKey(-10, 25, -1555)] = big_chunk;
    // chunks[packKey(435, 2, 152)] = small_chunk;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 12);

    Chunk* chunk_test = new Chunk({16, 16, 16}, {1, 1, 1});

    // Chunk* test_chunk = chunks[packKey(1, 1, 1)];

    // Chunk* chunk_to_draw = chunks[packKey(1, 2, 152)];
    // std::cout << chunk_to_draw << std::endl;


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
        // window->draw(grid, camera);


        // chunk->clear();

        // for (int x = 0; x < chunk->size.x; x++) {
        //     for (int z = 0; z < chunk->size.z; z++) {
        //         float x_norm = (float)x / chunk->size.x;
        //         float z_norm = (float)z / chunk->size.z;
        //         float pi = 3.14159265f;
                
        //         // normalized height [0,1]
        //         float y_norm = (sin(x_norm * 2 * pi + timer) + sin(z_norm * 2 * pi + timer) + 2.0f) / 4.0f;

        //         int y_max = std::min((int)(y_norm * chunk->size.y), chunk->size.y - 1);

        //         // Make all voxels below y_max visible
        //         for (int y = 0; y <= y_max; y++) {
        //             chunk->voxels[chunk->idx(x, y, z)].visible = true;
        //         }
        //     }
        // }
        // chunk_test->update_mesh();

        voxel_grid->update(camera);
        window->draw(voxel_grid, camera);
        // window->draw(chunk_test, camera);


        // cube->position.x += 1.0 * delta_time;

        window->swap_buffers();
        engine->poll_events();
    }
}
