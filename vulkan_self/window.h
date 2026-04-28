#pragma once
#include <string>
#include <cstdint>
#include <utility>
#include <iostream>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "glfw_context.h"

class Window {
public:
    _XCLASS_NAME(Window);

    Window() = delete;
    Window(const GlfwContext&, uint32_t width, uint32_t height, std::string_view title);
    ~Window();
    void destroy();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    GLFWwindow* handle() const;

    bool should_close() const;
    void poll_events() const;

    uint32_t width() const;
    uint32_t height() const;
    const std::string& title() const;

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::string m_title;

    GLFWwindow* m_window = nullptr;
};
