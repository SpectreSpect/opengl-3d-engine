#include "window.h"

Window::Window(int width, int height, std::string title) {
    this->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    this->width = width;
    this->height = height;
    if (!this->window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return;
    }
}

Window::~Window() {
    glfwDestroyWindow(window);
}


void Window::swap_buffers() {
    glfwSwapBuffers(window);
}

bool Window::is_open(){
    return !glfwWindowShouldClose(window);
}

float Window::get_aspect_ratio() {
    return width / height;
}

float Window::get_fbuffer_aspect_ratio() {
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    
    if (fbHeight == 0) 
        fbHeight = 1;

    return (float)fbWidth / float(fbHeight);
}

void Window::clear_color(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::draw(Drawable* drawable, Camera* camera, Program* program) {
    float aspect = get_fbuffer_aspect_ratio();

    RenderState states;
    states.vp = camera->get_projection_matrix(aspect) * camera->get_view_matrix();
    states.transform = glm::mat4(1.0f);
    states.program = program;
    states.camera = camera;

    drawable->draw(states);

    // glm::mat4 projection = camera->get_projection_matrix(aspect);
    // glm::mat4 view = camera->get_view_matrix();
    // glm::mat4 model = ???
    // glm::mat4 mvp = projection * view * model;

    // program->use();
    // program->set_mat4("uMVP", mvp);

    // mesh->draw();
}