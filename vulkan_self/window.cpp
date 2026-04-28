#include "window.h"

Window::Window(const GlfwContext&, uint32_t width, uint32_t height, std::string_view title)
    :   m_width(width), m_height(height), m_title(title) {
    LOG_METHOD();

    logger.log() << "GLFW version: " << clr(glfwGetVersionString(), "#ffaa2c") << "\n";

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        this->m_title.c_str(),
        nullptr,
        nullptr
    );

    logger.check(m_window, "Failed to create GLFW window");
}

Window::~Window() {
    destroy();
}

void Window::destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

Window::Window(Window&& other) noexcept
    :   m_window(std::exchange(other.m_window, nullptr)),
        m_width(std::exchange(other.m_width, 0)),
        m_height(std::exchange(other.m_height, 0)),
        m_title(std::move(other.m_title)) {}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();

        m_window = std::exchange(other.m_window, nullptr);
        m_width = std::exchange(other.m_width, 0);
        m_height = std::exchange(other.m_height, 0);
        m_title = std::move(other.m_title);
    }

    return *this;
}

GLFWwindow* Window::handle() const { 
    return m_window; 
}

bool Window::should_close() const {
    return glfwWindowShouldClose(m_window);
}

void Window::poll_events() const {
    glfwPollEvents();
}

uint32_t Window::width() const {
    return m_width;
}

uint32_t Window::height() const {
    return m_height;
}

const std::string& Window::title() const {
    return m_title;
}
