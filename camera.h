#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class Camera{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float fov;
    // float movment_speed;
        
    float yaw = -90.0f;
    float pitch = 0.0f;
    // float lastX;
    // float lastY;
    bool firstMouse = true;

    Camera(
        glm::vec3 pos = {0.0f, 1.0f, 5.0f},
        glm::vec3 up_vec = {0.0f, 1.0f, 0.0f},
        float fov_deg = 60.0f
        // float movment_speed = 5.0
    ) : position(pos), up(up_vec), fov(fov_deg)
    {
        front = {0.0f, 0.0f, -1.0f};
    }

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix(float aspect_ratio) const;
    // void processMouseMovement(float xoffset, float yoffset);

    // void update_default_camera_controls();

    // void move_forward(float dt) {
    //     position += front * movment_speed * dt;
    // }

    // void move_backward(float dt) {
    //     position -= front * movment_speed * dt;
    // }

    // void move_right(float dt) {
    //     glm::vec3 right = glm::normalize(glm::cross(front, up));
    //     position += right * movment_speed * dt;
    // }

    // void move_left(float dt) {
    //     glm::vec3 right = glm::normalize(glm::cross(front, up));
    //     position -= right * movment_speed * dt;
    // }

    // void move_up(float dt) {
    //     position += up * movment_speed * dt;
    // }

    // void move_down(float dt) {
    //     position -= up * movment_speed * dt;
    // }
};