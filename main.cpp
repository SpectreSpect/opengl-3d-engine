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
#include "third_person_camera_controller.h"


float clear_col[4] = {0, 0, 0, 1};

class Limb : public Drawable, public Transformable {
public:
    std::vector<float> bone_lengths;
    float limb_length = 0;
    std::vector<LineInstance> bones;
    std::vector<LineInstance> staright_line_instances;
    glm::vec3 start = glm::vec3(0, 0, 0);
    glm::vec3 end = glm::vec3(1, 0, 0);
    glm::vec3 initial_dir;
    Line limb_line;
    Line straight_line;

    Limb(int num_bones, std::vector<float> bone_lengths, glm::vec3 dir) {
        this->bone_lengths = bone_lengths;
        for (int i = 0; i < this->bone_lengths.size(); i++) 
            limb_length += this->bone_lengths[i];
        
        num_bones = std::max(num_bones, 2);
        bones = std::vector<LineInstance>(num_bones, {glm::vec3(0), glm::vec3(0)});
        staright_line_instances = std::vector<LineInstance>(1, {glm::vec3(0), glm::vec3(0)});
        straight_line.color = glm::vec4(0, 0, 1, 1);
        limb_line.color = glm::vec4(0.960784314f, 0.870588235f, 0.305882353f, 1.0f);
        initial_dir = dir;
        init();
    }

    void init() {
        glm::vec3 last_joint = start;
        initial_dir = glm::normalize(initial_dir);
        for (int i = 0; i < bones.size(); i++) {
            glm::vec3 limb_end = last_joint;
            limb_end = last_joint + initial_dir * bone_lengths[i];
            bones[i] = {last_joint, limb_end};
            last_joint = limb_end;
        }
    }

    void update_straight_line() {
        staright_line_instances[0].p0 = start;
        staright_line_instances[0].p1 = end;
        straight_line.set_lines(staright_line_instances);
    }

    void backward_pass() {
        bones.back().p1 = end;
        for (int i = bones.size() - 1; i >= 0; i--) {
            glm::vec3 dir = glm::normalize(bones[i].p0 - bones[i].p1); 
            bones[i].p0 = bones[i].p1 + dir * bone_lengths[i];

            if (i > 0)
                bones[i-1].p1 = bones[i].p0;
        }
        limb_line.set_lines(bones);
    }

    void forward_pass() {
        bones.front().p0 = start;
        for (int i = 0; i < bones.size(); i++) {
            glm::vec3 dir = glm::normalize(bones[i].p1 - bones[i].p0); 
            bones[i].p1 = bones[i].p0 + dir * bone_lengths[i];
            if (i + 1 < bones.size())
                bones[i+1].p0 = bones[i].p1;
        }
        limb_line.set_lines(bones);
    }

    virtual void draw(RenderState state) {
        state.transform *= get_model_matrix();
        limb_line.draw(state);
        // straight_line.draw(state);
    }
};


class Leg : public Limb{
public:
    float step_speed;
    float step_size;
    glm::vec3 step_middle;
    bool moving;
    Leg(int num_bones, std::vector<float> bone_lengths, glm::vec3 step_middle, float step_speed, float step_size, bool moving) : Limb(num_bones, bone_lengths, glm::vec3(0, 1, 0)) {
        this->step_speed = step_speed;
        this->step_size = step_size;
        this->step_middle = step_middle;
        this->moving = moving;
        end = this->step_middle;
        init();
    }

    void update(glm::vec3 velocity, float delta_time) {
        float movement_speed = glm::length(velocity);
        glm::vec3 movement_dir = movement_speed > 1e-6f
            ? velocity / movement_speed
            : glm::vec3(0.0f);

        glm::vec3 step_front = step_middle + movement_dir * step_size * 0.5f;
        glm::vec3 step_back  = step_middle - movement_dir * step_size * 0.5f;

        if (movement_speed <= 1e-6f) {
            glm::vec3 to_middle = step_middle - end;
            float dist_to_middle = glm::length(to_middle);

            if (dist_to_middle > 1e-6f) {
                glm::vec3 middle_dir = to_middle / dist_to_middle;
                float frame_step = step_speed * delta_time;
                end += middle_dir * std::min(frame_step, dist_to_middle);
            }

            moving = false;
        }
        else if (!moving) {
            // stance phase: keep foot roughly planted in world,
            // so in spider-local coordinates it moves backward
            float frame_move = movement_speed * delta_time;
            float dist_to_back = glm::distance(step_back, end);

            end -= movement_dir * std::min(frame_move, dist_to_back);

            float dist_to_front = glm::distance(step_front, end);
            if (dist_to_front >= step_size) {
                moving = true;
                // DO NOT snap: end = step_back;
            }
        }
        else {
            // swing phase: move foot toward front target
            glm::vec3 to_front = step_front - end;
            float dist = glm::length(to_front);

            if (dist > 1e-6f) {
                glm::vec3 dir_to_front = to_front / dist;
                float frame_step = step_speed * delta_time;

                if (dist <= frame_step) {
                    end = step_front;
                    moving = false;
                } else {
                    end += dir_to_front * frame_step;
                }
            } else {
                end = step_front;
                moving = false;
            }
        }

        end.y = 0.0f;

        backward_pass();
        forward_pass();
        backward_pass();
        forward_pass();

        update_straight_line();
    }   
};


class Spider : public Drawable, public Transformable {
public:
    Sphere body;
    // std::vector<Limb*> limbs;
    std::vector<Leg*> legs;

    std::vector<bool> moving_limb;
    glm::vec3 velocity = glm::vec3(0, 0, 0);
    Spider(int num_legs, int num_bones, float bone_length) {
        float pi = glm::pi<float>();
        std::vector<float> bone_lengths(num_bones, bone_length);
        for (int i = 0; i < num_legs; i++) {
            float angle = ((float)i / num_legs) * 2.0f * pi;
            glm::vec3 dir = glm::normalize(glm::vec3(glm::cos(angle), 0, glm::sin(angle)));
            glm::vec3 initial_end_dir = dir;
            initial_end_dir.y = 0;

            float step_size = 2;
            glm::vec3 middle_step = initial_end_dir * 3.0f;
            // glm::vec3 step_start = initial_end_pos;
            // step_start.x += step_size * 0.5f;

            bool moving = i % 2 == 0;
            // bool moving = false;

            Leg* leg = new Leg(num_bones, bone_lengths, middle_step, 10.0f, step_size, moving);

            legs.push_back(leg);
            moving_limb.push_back(false);
        }
    }

    void update(float delta_time) {
        for (int i = 0; i < legs.size(); i++)
            legs[i]->update(velocity, delta_time);

        // legs[1]->update(velocity);
        
        position += velocity * delta_time;
        
        // for (int i = 0; i < limbs.size(); i++) {
        //     limbs[i]->start = body.mesh->position;
        //     float step_speed = 0.1f;
        //     float max_dist = 3.0f;
        //     // glm::vec3 potential_end = limbs[i]->start + limbs[i]->initial_dir * limbs[i]->limb_length * (float)limbs[i]->bones.size();
        //     glm::vec3 potential_end = limbs[i]->start + limbs[i]->initial_dir * 2.0f;
        //     potential_end.y = 0;
        //     if (!moving_limb[i]) {
        //         if (glm::distance(potential_end, limbs[i]->end) >= max_dist)
        //             moving_limb[i] = true;
        //             // limbs[i]->end = potential_end;
        //         // limbs[i]->end.y = 0;
        //     } else {
        //         float dist = glm::distance(potential_end, limbs[i]->end);
        //         glm::vec3 limb_movement_dir = glm::normalize(potential_end - limbs[i]->end);
        //         limbs[i]->end += limb_movement_dir * std::min(step_speed, dist);

        //         float new_dist = glm::distance(potential_end, limbs[i]->end);

        //         if (new_dist <= 1e-12)
        //             moving_limb[i] = false;
        //     }
            
        //     limbs[i]->init();
        //     limbs[i]->limb_line.set_lines(limbs[i]->bones);
        //     limbs[i]->backward_pass();
        //     limbs[i]->forward_pass();
        //     limbs[i]->backward_pass();
        //     limbs[i]->forward_pass();
        //     limbs[i]->update_straight_line();
        // }
    }

    virtual void draw(RenderState state) {
        state.transform *= get_model_matrix();

        body.mesh->draw(state);
        for (int i = 0; i < legs.size(); i++) {
            legs[i]->draw(state);
        }
    }
};

int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);

    Camera camera;
    window.set_camera(&camera);
    // FPSCameraController camera_controller = FPSCameraController(&camera);

    // Camera camera;
    ThirdPersonController camera_controller(&camera);

    camera_controller.target_position = glm::vec3(0.0f, 0.0f, 0.0f);
    camera_controller.distance = 15.0f;
    camera_controller.look_offset = glm::vec3(0.0f, 0.0f, 0.0f);
    
    // camera_controller.speed = 50;

    Sphere sphere = Sphere();
    sphere.mesh->scale = sphere.mesh->scale / 1.5f;

    Sphere sphere2 = Sphere();
    // sphere.mesh->scale = sphere.mesh->scale / 1.5f;


    glm::vec3 limb_start = glm::vec3(0, 2, 0);
    glm::vec3 limb_end = glm::vec3(5, 0, 0);

    std::vector<LineInstance> straight_line_instances;
    straight_line_instances.push_back({limb_start, limb_end});

    Line straight_line;
    straight_line.set_lines(straight_line_instances);

    float limb_lenght = 2;
    std::vector<LineInstance> joints;
    glm::vec3 last_joint = limb_start;
    for (int i = 0; i < 4; i++) {
        glm::vec3 limb_end = last_joint;
        limb_end.x += limb_lenght;
        joints.push_back({last_joint, limb_end});
        last_joint = limb_end;
    }

    joints.back().p1 = limb_end;
    for (int i = joints.size() - 1; i >= 1; i--) {
        glm::vec3 dir = glm::normalize(joints[i].p0 - joints[i].p1); 
        joints[i].p0 = joints[i].p1 + dir * limb_lenght;

        if (i != 1)
            joints[i-1].p1 = joints[i].p0;
    }

    

    // joints.back().p0 = limb_start;
    // for (int i = 0; i < joints.size() - 1; i++) {
    //     glm::vec3 dir = glm::normalize(joints[i].p1 - joints[i].p0); 
    //     joints[i].p1 = joints[i].p0 + dir * limb_lenght;
    //     joints[i+1].p0 = joints[i].p1;
    // }

    for (int i = 0; i < joints.size() - 1; i++) {
        float dist = glm::distance(joints[i].p0, joints[i].p1);
        std::cout << dist << std::endl;
    }


    // Line limb;
    // limb.set_lines(joints);
    // limb.color = glm::vec4(0, 0, 1, 1);


    // Limb right_limb = Limb(4, 2, glm::vec3(1, 0.5, 0));
    // right_limb.start = glm::vec3(0, 4, 0);
    // right_limb.end = glm::vec3(5, 0, 0);

    // Limb left_limb = Limb(4, 2, glm::vec3(-1, 0.5, 0));
    // left_limb.start = glm::vec3(0, 4, 0);
    // left_limb.end = glm::vec3(-5, 0, 0);

    glm::vec3 origin = glm::vec3(0, 3, 0);
    

    PBRSkybox skybox = PBRSkybox(engine.texture_manager->st_peters_square_night_4k);



    Spider spider = Spider(5, 4, 2.0f);
    spider.velocity = glm::vec3(0, 0, 0);

    float rel_thresh = 1.5f;
    float last_frame = 0.0f;
    float timer = 0.0f;
    int cur_frame = 100;
    bool map_initialized = false;
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - last_frame;
        timer += delta_time;
        last_frame = currentFrame;   
        float fps = 1.0f / delta_time;
        
        camera_controller.update(&window, delta_time);

        engine.update_lighting_system(camera, window);
        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        window.draw(&skybox, &camera);

        skybox.attach_environment(*engine.default_program);
        skybox.attach_environment(*engine.default_circle_program);

        if (glm::length(camera_controller.move_direction) > 0.0f) {
            spider.velocity = camera_controller.move_direction * 5.0f;
        }else {
            spider.velocity = glm::vec3(0.0f);
        }
        // spider.velocity.x = 5;

        // spider.velocity.x += timer * 0.000001f;
        
        // right_limb.start = origin;
        // right_limb.start.x += std::cos(timer * 5) * 2;
        // right_limb.start.y += std::sin(timer * 5) * 2;

        // left_limb.start = right_limb.start;
        // sphere.mesh->position = right_limb.start;
        

        // right_limb.end.x = ((std::cos(timer) + 1.0f) / 2.0f) * 7.0f;
        // left_limb.end.x = ((std::cos(timer) + 1.0f) / 2.0f) * -7.0f;
        
        // right_limb.init();
        // right_limb.backward_pass();
        // right_limb.forward_pass();
        // right_limb.backward_pass();
        // right_limb.forward_pass();
        // right_limb.update_straight_line();

        // left_limb.init();
        // left_limb.backward_pass();
        // left_limb.forward_pass();
        // left_limb.backward_pass();
        // left_limb.forward_pass();
        // left_limb.update_straight_line();


        // window.draw(sphere.mesh, &camera);
        // window.draw(&straight_line, &camera);
        // window.draw(&left_limb, &camera);
        // window.draw(&right_limb, &camera);

        // spider.body.mesh->position.y = ((std::cos(timer * 5.0f) + 1.0f) / 2.0f) * 5.0f;
        // spider.body.mesh->position.x = timer * 1;

        spider.update(delta_time);
        window.draw(&spider, &camera);
        window.draw(sphere2.mesh, &camera);
        

        camera_controller.target_position = spider.position;
        
        

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "fps: %.3f", fps);
        ImGui::InputInt("current frame", &cur_frame);
        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);
        ImGui::End();

        ui::end_frame();
        

        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
