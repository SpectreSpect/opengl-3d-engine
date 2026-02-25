#include "engine3d.h"
#include "window.h"
#include "camera.h"

Engine3D::Engine3D(){
    init();

    mesh_manager = new MeshManager();
    material_manager = new MaterialManager();
    light_sources = std::vector<LightSource>(max_num_light_sources);

    lights_in_clusters = std::vector<std::vector<size_t>>(num_clusters.x * num_clusters.y * num_clusters.z);
    for (auto &v : lights_in_clusters) {
        v.reserve(max_lights_per_cluster);
    }
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
    
    size_t total_clusters_count = num_clusters.x * num_clusters.y * num_clusters.z;
    size_t lights_in_clusters_size = total_clusters_count * max_lights_per_cluster;
    lights_in_clusters_ssbo = SSBO(sizeof(unsigned int) * lights_in_clusters_size, GL_DYNAMIC_DRAW);
    num_lights_in_clusters_ssbo = SSBO(sizeof(unsigned int) * total_clusters_count, GL_DYNAMIC_DRAW);
    cluster_aabbs_ssbo = SSBO(sizeof(AABB) * total_clusters_count, GL_DYNAMIC_DRAW);

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

void Engine3D::update_lights_in_clusters() {
    
}

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

// void Engine3D::computeSliceDistancesLinear(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out) {
//     out.resize(zSlices + 1);
//     for (unsigned i = 0; i <= zSlices; ++i) {
//         float t = float(i) / float(zSlices);
//         out[i] = glm::mix(nearPlane, farPlane, t);
//     }
// }

// // Helper: compute logarithmic slice distances (good for large depth ranges)
// void Engine3D::computeSliceDistancesLog(float nearPlane, float farPlane, unsigned zSlices, std::vector<float>& out) {
//     out.resize(zSlices + 1);
//     float lnN = std::log(nearPlane);
//     float lnF = std::log(farPlane);
//     for (unsigned i = 0; i <= zSlices; ++i) {
//         float t = float(i) / float(zSlices);
//         out[i] = std::exp(glm::mix(lnN, lnF, t));
//     }
// }


// void Engine3D::create_clusters_full(
//     std::vector<AABB>& out_cluster_cells,
//     glm::uvec3 numClusters,        // xTiles, yTiles, zSlices (use unsigned ints)
//     float fovY_radians,
//     float aspect,
//     const std::vector<float>& sliceDistances) 
// {
//     unsigned xTiles = numClusters.x;
//     unsigned yTiles = numClusters.y;
//     unsigned zSlices = numClusters.z;
//     out_cluster_cells.clear();
//     out_cluster_cells.resize((size_t)xTiles * yTiles * zSlices);

//     const float tanHalfFovY = tanf(fovY_radians * 0.5f);

//     for (unsigned z = 0; z < zSlices; ++z) {
//         float d0 = sliceDistances[z];     // near bound for slice (positive)
//         float d1 = sliceDistances[z+1];   // far  bound
//         // half heights/widths at both depths
//         float hh0 = d0 * tanHalfFovY;
//         float hh1 = d1 * tanHalfFovY;
//         float hw0 = hh0 * aspect;
//         float hw1 = hh1 * aspect;

//         for (unsigned y = 0; y < yTiles; ++y) {
//             // normalized v coordinates (0..1). v=0 bottom, v=1 top
//             float v0 = float(y) / float(yTiles);
//             float v1 = float(y + 1) / float(yTiles);

//             for (unsigned x = 0; x < xTiles; ++x) {
//                 float u0 = float(x) / float(xTiles);
//                 float u1 = float(x + 1) / float(xTiles);

//                 // compute 8 corners of frustum cell in view space (z = -d)
//                 glm::vec3 corners[8];

//                 // near plane corners (d0)
//                 float xs0 = (u0 * 2.0f - 1.0f) * hw0;
//                 float xs1 = (u1 * 2.0f - 1.0f) * hw0;
//                 float ys0 = (v0 * 2.0f - 1.0f) * hh0;
//                 float ys1 = (v1 * 2.0f - 1.0f) * hh0;
//                 corners[0] = glm::vec3(xs0, ys0, -d0);
//                 corners[1] = glm::vec3(xs1, ys0, -d0);
//                 corners[2] = glm::vec3(xs1, ys1, -d0);
//                 corners[3] = glm::vec3(xs0, ys1, -d0);

//                 // far plane corners (d1)
//                 xs0 = (u0 * 2.0f - 1.0f) * hw1;
//                 xs1 = (u1 * 2.0f - 1.0f) * hw1;
//                 ys0 = (v0 * 2.0f - 1.0f) * hh1;
//                 ys1 = (v1 * 2.0f - 1.0f) * hh1;
//                 corners[4] = glm::vec3(xs0, ys0, -d1);
//                 corners[5] = glm::vec3(xs1, ys0, -d1);
//                 corners[6] = glm::vec3(xs1, ys1, -d1);
//                 corners[7] = glm::vec3(xs0, ys1, -d1);

//                 // create AABB from these 8 points
//                 glm::vec3 bmin( std::numeric_limits<float>::infinity() );
//                 glm::vec3 bmax( -std::numeric_limits<float>::infinity() );
//                 for (int i = 0; i < 8; ++i) {
//                     bmin = glm::min(bmin, corners[i]);
//                     bmax = glm::max(bmax, corners[i]);
//                 }

//                 // small epsilon padding (optional) to avoid numerical misses:
//                 const float eps = 1e-4f * (d0 + d1) * 0.5f; // scale with depth a bit
//                 bmin -= glm::vec3(eps);
//                 bmax += glm::vec3(eps);

//                 size_t idx = size_t(x) + size_t(y) * xTiles + size_t(z) * xTiles * yTiles;
//                 out_cluster_cells[idx].min = bmin;
//                 out_cluster_cells[idx].max = bmax;
//             }
//         }
//     }
// }

// void Engine3D::create_clusters(std::vector<AABB>& out_cluster_cells, glm::vec3 num_clusters,
//                                float fovY_radians, float aspect, float nearPlane, float farPlane,
//                                bool useLogSlices = false)
// {
//     // convert glm::vec3 -> glm::uvec3
//     glm::uvec3 tiles = glm::uvec3((unsigned)num_clusters.x, (unsigned)num_clusters.y, (unsigned)num_clusters.z);

//     std::vector<float> sliceDistances;
//     if (useLogSlices) computeSliceDistancesLog(nearPlane, farPlane, tiles.z, sliceDistances);
//     else              computeSliceDistancesLinear(nearPlane, farPlane, tiles.z, sliceDistances);

//     create_clusters_full(out_cluster_cells, tiles, fovY_radians, aspect, sliceDistances);
// }

void Engine3D::update_clusters(const std::vector<AABB> &clusters, const glm::mat4& view_matrix) {
    for (int light_id = 0; light_id < light_sources.size(); light_id++) {
        LightSource& light_source = light_sources[light_id];
        glm::vec3 light_view_pos = glm::vec3(view_matrix * glm::vec4(light_source.position.x, light_source.position.y, light_source.position.z, 1.0f));
        float radius = light_source.position.w;

        for (int cluster_id = 0; cluster_id < clusters.size(); cluster_id++) {    
            if (light_id == 0)
                lights_in_clusters[cluster_id].clear();

            bool intersects = sphereIntersectsAABB_ViewSpace(light_view_pos, radius, clusters[cluster_id]);

            if (intersects) {
                lights_in_clusters[cluster_id].push_back(light_id);
            }
        }
    }
}

void Engine3D::update_light_indices_for_clusters(ComputeProgram& light_indices_for_clusters_program, const Camera& camera) {
    unsigned int total_clusters_count = num_clusters.x * num_clusters.y * num_clusters.z;
    
    int x_count = math_utils::div_up_u32(total_clusters_count, 256u);;
    int y_count = light_sources.size();

    light_indices_for_clusters_program.set_uint("num_clusters", total_clusters_count);
    light_indices_for_clusters_program.set_uint("max_lights_per_cluster", max_lights_per_cluster);
    light_indices_for_clusters_program.set_mat4("view_matrix", camera.get_view_matrix());


    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, num_lights_in_clusters_ssbo.id_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER,
                    GL_R32UI,
                    GL_RED_INTEGER,
                    GL_UNSIGNED_INT,
                    &zero);

    cluster_aabbs_ssbo.bind_base(4);
    light_source_ssbo.bind_base(5);
    num_lights_in_clusters_ssbo.bind_base(6);
    lights_in_clusters_ssbo.bind_base(7);

    // GLuint q[2];
    // glGenQueries(2, q);
    // glQueryCounter(q[0], GL_TIMESTAMP);   
    light_indices_for_clusters_program.dispatch_compute(x_count, y_count, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    // glQueryCounter(q[1], GL_TIMESTAMP);

    // GLuint64 start_ns = 0, end_ns = 0;
    // glGetQueryObjectui64v(q[0], GL_QUERY_RESULT, &start_ns);
    // glGetQueryObjectui64v(q[1], GL_QUERY_RESULT, &end_ns);

    // double gpu_ms = double(end_ns - start_ns) * 1e-6;
    // std::cout << "GPU time: " << gpu_ms << " ms\n";

    // glDeleteQueries(2, q);
}

void Engine3D::set_cluster_aabbs(std::vector<AABB>& aabbs) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cluster_aabbs_ssbo.id_);
    glBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        aabbs.size() * sizeof(AABB),
        aabbs.data()
    );
}

bool Engine3D::sphereIntersectsAABB_ViewSpace(const glm::vec3 &centerVS, float radius, const AABB &aabb) {
    float d2 = 0.0f;
    if (centerVS.x < aabb.min.x) { float d = aabb.min.x - centerVS.x; d2 += d*d; }
    else if (centerVS.x > aabb.max.x) { float d = centerVS.x - aabb.max.x; d2 += d*d; }
    if (centerVS.y < aabb.min.y) { float d = aabb.min.y - centerVS.y; d2 += d*d; }
    else if (centerVS.y > aabb.max.y) { float d = centerVS.y - aabb.max.y; d2 += d*d; }
    if (centerVS.z < aabb.min.z) { float d = aabb.min.z - centerVS.z; d2 += d*d; }
    else if (centerVS.z > aabb.max.z) { float d = centerVS.z - aabb.max.z; d2 += d*d; }
    return d2 <= radius * radius;
}


void Engine3D::enable_depth_test() {
    glEnable(GL_DEPTH_TEST);
}

void Engine3D::poll_events() {
    glfwPollEvents();
}