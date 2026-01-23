cmake_minimum_required(VERSION 3.10)
project(app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: silence OpenGL CMP0072 warning on Ubuntu
if(POLICY CMP0072)
  cmake_policy(SET CMP0072 NEW)
endif()
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

add_executable(app
  main.cpp
  engine3d.cpp
  window.cpp
  vao.cpp
  vbo.cpp
  ebo.cpp
  vertex_shader.cpp
  fragment_shader.cpp
  program.cpp
  shader.cpp
  vertex_layout.cpp
  camera.cpp
  mesh.cpp
  transformable.cpp
  cube.cpp
  fps_camera_controller.cpp
  material_template.cpp
  material_manager.cpp
  imgui_layer.cpp
  voxel_engine/voxel.cpp
  voxel_engine/chunk.cpp
  voxel_engine/voxel_grid.cpp
  voxel_engine/voxel_editor.cpp
)

add_custom_command(TARGET app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/shaders"
          "$<TARGET_FILE_DIR:app>/shaders"
)

# ---------------- ImGui (FetchContent, no submodule) ----------------
include(FetchContent)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  # Pick a tag you want; keep it fixed for reproducible builds
  GIT_TAG v1.90.9
)

FetchContent_MakeAvailable(imgui)

# Compile ImGui directly into your target (core)
target_sources(app PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

# Optional: demo window (remove if you don't want it)
# target_sources(app PRIVATE ${imgui_SOURCE_DIR}/imgui_demo.cpp)

# Backends you use (GLFW + OpenGL3)
target_sources(app PRIVATE
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Make <imgui.h> and <imgui_impl_glfw.h> resolvable
target_include_directories(app PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

# ---------------- Other links ----------------
target_link_libraries(app PRIVATE
  glfw
  GLEW::GLEW
  glm::glm
  OpenGL::GL
)

# Windows-only defines/properties
if (WIN32)
  target_compile_definitions(app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_target_properties(app PROPERTIES WIN32_EXECUTABLE OFF)
endif()
