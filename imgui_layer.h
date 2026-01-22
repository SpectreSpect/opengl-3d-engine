#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

struct GLFWwindow;

namespace ui {
  void init(GLFWwindow* window);
  void begin_frame();
  void end_frame();
  void shutdown();
}
