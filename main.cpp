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


// glm::vec3 cameraPos   = glm::vec3(0.0f, 1.0f,  5.0f);
// glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
// glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

// float yaw   = -90.0f; // facing -Z initially
// float pitch =  0.0f;

// float lastX = (float)800 / 2.0f;
// float lastY = (float)600 / 2.0f;
// bool firstMouse = true;

// timing
// float deltaTime = 0.0f;
// float lastFrame = 0.0f;

// void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
//     if (firstMouse) {
//         lastX = (float)xpos;
//         lastY = (float)ypos;
//         firstMouse = false;
//     }
//     float xoffset = (float)xpos - lastX;
//     float yoffset = lastY - (float)ypos; // reversed
//     lastX = (float)xpos;
//     lastY = (float)ypos;

//     float sensitivity = 0.15f; // tweak
//     xoffset *= sensitivity;
//     yoffset *= sensitivity;

//     yaw   += xoffset;
//     pitch += yoffset;

//     if (pitch > 89.0f) pitch = 89.0f;
//     if (pitch < -89.0f) pitch = -89.0f;

//     glm::vec3 front;
//     front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
//     front.y = sin(glm::radians(pitch));
//     front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
//     cameraFront = glm::normalize(front);
// }
Camera* camera = new Camera();

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (camera->firstMouse) {
        camera->lastX = (float)xpos;
        camera->lastY = (float)ypos;
        camera->firstMouse = false;
    }

    float xoffset = (float)xpos - camera->lastX;
    float yoffset = camera->lastY - (float)ypos; // reversed
    camera->lastX = (float)xpos;
    camera->lastY = (float)ypos;

    camera->processMouseMovement(xoffset, yoffset);
}

// framebuffer size
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// void processInput(GLFWwindow *window, Camera* camera, float delta_time) {
//     float speed = camera * delta_time;
//     glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
//     if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += speed * cameraFront;
//     if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
//     if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= right * speed;
//     if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += right * speed;
//     if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += cameraUp * speed;
//     if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) cameraPos -= cameraUp * speed;
//     if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
// }

void processInput(GLFWwindow *window, Camera* camera, float delta_time) {
    glm::vec3 right = glm::normalize(glm::cross(camera->front, camera->up));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera->move_forward(delta_time);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera->move_backward(delta_time);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera->move_left(delta_time);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera->move_right(delta_time);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera->move_up(delta_time);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera->move_down(delta_time);
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
}


int main() {
    Engine3D* engine = new Engine3D();
    engine->init();

    Window* window = new Window(1280, 720, "3D visualization");
    engine->make_context_current(window);
    engine->init_glew();

    glfwSetInputMode(window->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window->window, mouse_callback);

    glfwSetFramebufferSizeCallback(window->window, framebuffer_size_callback);

    std::vector<float> vertices_vec = {
        // positions         // colors
        -1.0f,-1.0f,-1.0f,   0.8f,0.2f,0.2f,
         1.0f,-1.0f,-1.0f,   0.2f,0.8f,0.2f,
         1.0f, 1.0f,-1.0f,   0.2f,0.2f,0.8f,
        -1.0f, 1.0f,-1.0f,   0.8f,0.8f,0.2f,
        -1.0f,-1.0f, 1.0f,   0.2f,0.8f,0.8f,
         1.0f,-1.0f, 1.0f,   0.8f,0.2f,0.8f,
         1.0f, 1.0f, 1.0f,   0.6f,0.6f,0.6f,
        -1.0f, 1.0f, 1.0f,   0.3f,0.7f,0.4f
    };

    std::vector<unsigned int> indices_vec = {
        0,1,2, 2,3,0,
        1,5,6, 6,2,1,
        5,4,7, 7,6,5,
        4,0,3, 3,7,4,
        3,2,6, 6,7,3,
        4,5,1, 1,0,4
    };

    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add({0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0});
    vertex_layout->add({1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 3*sizeof(float)});

    VertexShader* vertex_shader = new VertexShader("../shaders/deafult_vertex.glsl");
    FragmentShader* fragment_shader = new FragmentShader("../shaders/deafult_fragment.glsl");

    Program* program = new Program(vertex_shader, fragment_shader);

    engine->enable_depth_test();


    
    engine->set_camera(camera);

    // Mesh* mesh = new Mesh(vertices_vec, indices_vec, program, vertex_layout);

    Cube* cube = new Cube();
    // Mesh* mesh = new Mesh(vertices, indices, sizeof(vertices), sizeof(indices), program, vertex_layout);

    float lastFrame = 0;
    while(window->is_open()) {
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window->window, camera, deltaTime);

        // window->clear_color({0.1f, 0.12f, 0.14f, 1.0f});
        window->clear_color({0.776470588f, 0.988235294f, 1.0f, 1.0f});

        window->draw(cube, camera, program);

        window->swap_buffers();
        engine->poll_events();
    }
}
