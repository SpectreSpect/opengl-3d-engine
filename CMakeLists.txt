cmake_minimum_required(VERSION 3.10)
project(app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

add_executable(app main.cpp engine3d.cpp window.cpp vao.cpp vbo.cpp ebo.cpp vertex_shader.cpp fragment_shader.cpp program.cpp shader.cpp vertex_layout.cpp camera.cpp mesh.cpp transformable.cpp cube.cpp fps_camera_controller.cpp)

target_link_libraries(app OpenGL::GL GLEW::GLEW glfw)
