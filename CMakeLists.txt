cmake_minimum_required(VERSION 3.10)
project(app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(imgui CONFIG REQUIRED)

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
  voxel_engine/voxel.cpp
  voxel_engine/chunk.cpp
  voxel_engine/voxel_grid.cpp
  imgui_layer.cpp
  voxel_rastorizator.cpp
  ui_elements/triangle_controller.cpp
  triangle.cpp
)

add_custom_command(TARGET app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/shaders"
          "$<TARGET_FILE_DIR:app>/shaders"
)

target_link_libraries(app PRIVATE
  glfw
  opengl32
  GLEW::GLEW
  imgui::imgui
)

target_compile_definitions(app PRIVATE 
  NOMINMAX 
  WIN32_LEAN_AND_MEAN
)

set_target_properties(app PROPERTIES WIN32_EXECUTABLE OFF)
