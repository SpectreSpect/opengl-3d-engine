#pragma once
#include "camera.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include "window.h"

class FPSCameraController {
public:
    Camera* camera;

    bool first_mouse = true;
    float last_x = 0.0f;
    float last_y = 0.0f;

    float yaw = -90.0f;
    float pitch = 0.0f;
    float mouse_sensitivity = 0.15f;
    float speed = 5.0f;

    FPSCameraController(Camera* camera);
    void update_keyboard(Window* window, float delta_time);
    void update_mouse(Window* window, float delta_time);
    void update(Window* window, float delta_time);


    void move_forward(float dt) {
        camera->position += camera->front * speed * dt;
    }

    void move_backward(float dt) {
        camera->position -= camera->front * speed * dt;
    }

    void move_right(float dt) {
        glm::vec3 right = glm::normalize(glm::cross(camera->front, camera->up));
        camera->position += right * speed * dt;
    }

    void move_left(float dt) {
        glm::vec3 right = glm::normalize(glm::cross(camera->front, camera->up));
        camera->position -= right * speed * dt;
    }

    void move_up(float dt) {
        camera->position += camera->up * speed * dt;
    }

    void move_down(float dt) {
        camera->position -= camera->up * speed * dt;
    }
};
