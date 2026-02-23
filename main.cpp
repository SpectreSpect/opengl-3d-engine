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
#include "hdr_frame_buffer.h"
#include "vf_program.h"


// float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};
float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    VertexShader* blur_vs = new VertexShader(executable_dir() / "shaders" / "blur_vs.glsl");
    FragmentShader* blur_fs = new FragmentShader(executable_dir() / "shaders" / "blur_fs.glsl");
    VfProgram* blur_program = new VfProgram(blur_vs, blur_fs);

    VertexShader* final_vs = new VertexShader(executable_dir() / "shaders" / "final_vs.glsl");
    FragmentShader* final_fs = new FragmentShader(executable_dir() / "shaders" / "final_fs.glsl");
    VfProgram* final_program = new VfProgram(final_vs, final_fs);


    HDRFrameBuffer frame_buffer = HDRFrameBuffer();
    frame_buffer.init(window.width, window.height);
    // frame_buffer.init_ping_pong(window.width / 2, window.height / 2);
    frame_buffer.init_ping_pong(window.width, window.height);

    // ShaderManager shader_manager = ShaderManager(executable_dir_str());

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;

    glm::vec3 prev_cam_pos = camera_controller.camera->position;

    float timer = 0;
    float lastFrame = 0;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    voxel_grid->update(&window, &camera);
    // sleep(2.0f);

    // Voxel voxel;
    // voxel.color = glm::vec3(0.0, 2.0, 0.0);
    // voxel.visible = true;
    // voxel_grid->set_voxel(voxel, glm::vec3(0, 20, 0));

    



    while(window.is_open()) {
        frame_buffer.bind();
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        

        camera_controller.update(&window, delta_time);

        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});


        

        voxel_grid->update(&window, &camera);
        window.draw(voxel_grid, &camera);


        // voxel_grid->edit_voxels([&](VoxelEditor voxel_editor) {
        //     Voxel voxel;
        //     voxel.color = glm::vec3(1.0f, 0.0f, 0.0f);
        //     voxel.visible = true;
        //     voxel.curvature = 1.0f;
        //     // voxel_grid->set_voxel(voxel, glm::vec3(0, 20, 0));
        //     voxel_editor.set(glm::vec3(0, 10, 0), voxel);
        // });
        
        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        // ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        ImGui::End();

        ui::end_frame();
        frame_buffer.unbind();

        frame_buffer.blur(blur_program);
        frame_buffer.render_final(final_program);

        window.swap_buffers();
        engine.poll_events();
        
    }
    
    ui::shutdown();
}
