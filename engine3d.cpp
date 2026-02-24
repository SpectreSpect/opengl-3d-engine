#include "engine3d.h"
#include "window.h"

Engine3D::Engine3D(){
    init();

    mesh_manager = new MeshManager();
    material_manager = new MaterialManager();
    light_sources = std::vector<LightSource>(max_num_light_sources);
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

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // Для дебага

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

    mesh_manager->load_cube();
    material_manager->load_blinn_phong();

    shader_manager = new ShaderManager(executable_dir_str());


    // default_vertex_shader = new VertexShader(default_vertex_shader_path);
    // default_fragment_shader = new FragmentShader(default_fragment_shader_path);
    // default_program = new Program(default_vertex_shader, default_fragment_shader);
    
    default_vertex_shader = new VertexShader(default_vertex_shader_path);
    default_fragment_shader = new FragmentShader(default_fragment_shader_path);
    default_program = new VfProgram(default_vertex_shader, default_fragment_shader);
    
    // default_program = new VfProgram(&shader_manager->default_vertex_shader, &shader_manager->default_fragment_shader);

    // default_line_vertex_shader = new VertexShader(default_line_vertex_shader_path);
    // default_line_fragment_shader = new FragmentShader(default_line_fragment_shader_path);
    default_line_program = new VfProgram(&shader_manager->default_line_vertex_shader, &shader_manager->default_line_fragment_shader);

    // default_point_vertex_shader = new VertexShader(default_point_vertex_shader_path);
    // default_point_fragment_shader = new FragmentShader(default_point_fragment_shader_path);
    default_point_program = new VfProgram(&shader_manager->default_point_vertex_shader, &shader_manager->default_point_fragment_shader);

    light_source_ssbo = SSBO(sizeof(LightSource) * max_num_light_sources, GL_DYNAMIC_DRAW, light_sources.data());

    return 1;
}

void Engine3D::set_window(Window* window) {
    if (!window || !window->window) {
        std::cerr << "set_window: GLFWwindow is null (create failed)\n";
        return;
    }

    glfwMakeContextCurrent(window->window);

    if (init_glew() != 1) {
        std::cerr << "OpenGL init failed\n";
        return;
    }

    enable_depth_test();

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                      GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_TRUE);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);


    // glDebugMessageCallback(
    //     [](GLenum source, GLenum type, GLuint id, GLenum severity,
    //     GLsizei length, const GLchar* message, const void* userParam)
    //     {
    //         std::cerr << "[GL DEBUG] " << message << "\n";
    //     },
    //     nullptr
    // );
}

// void Engine3D::set_camera(Camera* camera) {
//     this->camera = camera;
// }

void Engine3D::set_light_source(size_t id, LightSource light_source) {
    light_sources[id] = light_source;
    dirty_lights.insert(id);
}

void Engine3D::update_light_sources() {
    if (dirty_lights.empty())
        return;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, light_source_ssbo.id_);

    LightSource* gpu_lights = (LightSource*)glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER,
        0,
        light_sources.size() * sizeof(LightSource),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT
    );
    
    for (size_t light_id : dirty_lights) {
        gpu_lights[light_id] = light_sources[light_id];
    }

    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    
    dirty_lights.clear();
}

void Engine3D::enable_depth_test() {
    glEnable(GL_DEPTH_TEST);
}

void Engine3D::poll_events() {
    glfwPollEvents();
}