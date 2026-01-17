#include "engine3d.h"


Engine3D::Engine3D(){

}

Engine3D::~Engine3D(){
    glfwTerminate();
}

int Engine3D::init() {
        if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return -1;
    }

    // Request OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    return 1;
}

int Engine3D::init_glew() {
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewErr) << "\n";
        return -1;
    }
    glGetError();
    return 1;
}


void Engine3D::make_context_current(Window* window) {
    glfwMakeContextCurrent(window->window);
}

void Engine3D::set_camera(Camera* camera) {
    this->camera = camera;
}

void Engine3D::enable_depth_test() {
    glEnable(GL_DEPTH_TEST);
}

void Engine3D::poll_events() {
    glfwPollEvents();
}