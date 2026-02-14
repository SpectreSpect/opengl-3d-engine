// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <cmath>

#include "engine3d.h"

#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include "vertex_layout.h"
#include "program.h"
#include "camera.h"
#include "mesh.h"
#include "cube.h"
#include "window.h"
#include "fps_camera_controller.h"
#include "voxel_engine/chunk.h"
#include "voxel_engine/voxel_grid.h"
#include "imgui_layer.h"
#include "voxel_rastorizator.h"
#include "ui_elements/triangle_controller.h"
#include "triangle.h"
#include "a_star/a_star.h"
#include "line.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/reeds_shepp.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "point.h"
#include "point_cloud/point_cloud_frame.h"
#include "point_cloud/point_cloud_video.h"

struct PointXYZRGBI {
    float x, y, z;
    uint8_t r, g, b;
    float intensity;
};

// struct Frame {
//     uint64_t timestamp_ns = 0;
//     uint32_t flags = 0;        // 1=rgb, 2=intensity
//     std::vector<PointXYZRGBI> points;
// };

// static Frame read_frame_bin(const std::filesystem::path& path)
// {
//     std::ifstream in(path, std::ios::binary);
//     if (!in) throw std::runtime_error("Failed to open: " + path.string());

//     Frame f{};
//     uint32_t count = 0;

//     in.read(reinterpret_cast<char*>(&f.timestamp_ns), sizeof(uint64_t));
//     in.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
//     in.read(reinterpret_cast<char*>(&f.flags), sizeof(uint32_t));
//     if (!in) throw std::runtime_error("Bad header in: " + path.string());

//     const bool has_rgb = (f.flags & 1u) != 0;
//     const bool has_intensity = (f.flags & 2u) != 0;

//     size_t bpp = 3 * sizeof(float);
//     if (has_rgb) bpp += 3 * sizeof(uint8_t);
//     if (has_intensity) bpp += sizeof(float);

//     std::vector<uint8_t> buf(size_t(count) * bpp);
//     in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
//     if (!in) throw std::runtime_error("Unexpected EOF in: " + path.string());

//     f.points.resize(count);

//     const uint8_t* p = buf.data();
//     for (uint32_t i = 0; i < count; ++i) {
//         std::memcpy(&f.points[i].x, p, 4); p += 4;
//         std::memcpy(&f.points[i].y, p, 4); p += 4;
//         std::memcpy(&f.points[i].z, p, 4); p += 4;
        
//         float y = f.points[i].y;
//         float z = f.points[i].z;

//         f.points[i].x = -f.points[i].x;
//         f.points[i].y = z;
//         f.points[i].z = y;


//         //   glm::vec3 pos_0 = glm::vec3(-point_0.x, point_0.z, point_0.y);

//         f.points[i].r = f.points[i].g = f.points[i].b = 255;
//         f.points[i].intensity = 0.0f;

//         if (has_rgb) {
//             f.points[i].r = *p++;
//             f.points[i].g = *p++;
//             f.points[i].b = *p++;
//         }
//         if (has_intensity) {
//             std::memcpy(&f.points[i].intensity, p, 4); p += 4;
//         }
//     }

//     return f;
// }

// Very small CSV parser for: frame_id,timestamp_ns,filename,point_count
// Skips empty/bad lines. Supports optional header row.
// static std::vector<IndexEntry> read_index_csv(const std::filesystem::path& index_path)
// {
//     std::ifstream in(index_path);
//     if (!in) throw std::runtime_error("Failed to open: " + index_path.string());

//     std::vector<IndexEntry> entries;
//     std::string line;

//     while (std::getline(in, line)) {
//         if (line.empty()) continue;

//         std::stringstream ss(line);
//         std::string a, b, c, d;

//         if (!std::getline(ss, a, ',')) continue;
//         if (!std::getline(ss, b, ',')) continue;
//         if (!std::getline(ss, c, ',')) continue;
//         if (!std::getline(ss, d, ',')) continue;

//         // If it's a header (non-numeric first field), skip it
//         auto is_uint = [](const std::string& s) {
//             if (s.empty()) return false;
//             for (char ch : s) if (ch < '0' || ch > '9') return false;
//             return true;
//         };
//         if (!is_uint(a) || !is_uint(b) || !is_uint(d)) continue;

//         IndexEntry e;
//         e.frame_id = std::stoull(a);
//         e.timestamp_ns = std::stoull(b);
//         e.filename = c;
//         e.point_count = static_cast<uint32_t>(std::stoul(d));
//         entries.push_back(std::move(e));
//     }

//     std::sort(entries.begin(), entries.end(),
//               [](const IndexEntry& x, const IndexEntry& y) { return x.timestamp_ns < y.timestamp_ns; });

//     return entries;
// }

static glm::vec3 hsv_to_rgb(float h, float s, float v) {
    h = std::fmod(h, 1.0f);
    if (h < 0.0f) h += 1.0f;

    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r=0,g=0,b=0;
    float hp = h * 6.0f;

    if      (hp < 1.0f) { r=c; g=x; b=0; }
    else if (hp < 2.0f) { r=x; g=c; b=0; }
    else if (hp < 3.0f) { r=0; g=c; b=x; }
    else if (hp < 4.0f) { r=0; g=x; b=c; }
    else if (hp < 5.0f) { r=x; g=0; b=c; }
    else                { r=c; g=0; b=x; }

    return {r + m, g + m, b + m};
}

static float clamp01(float x) {
    return std::max(0.0f, std::min(1.0f, x));
}




// float clear_col[4] = {0.776470588f, 0.988235294f, 1.0f, 1.0f};
float clear_col[4] = {0.1f, 0.1f, 0.1f, 1.0f};
// float clear_col[4] = {0.0f, 0.0f, 0.0f, 1.0f};


std::vector<LineInstance> get_arrow(glm::vec3 p0, glm::vec3 p1) {
    LineInstance middle{p0, p1};

    glm::vec3 d = p1 - p0;
    float len = glm::length(d);
    if (len < 1e-6f) return { middle };

    glm::vec3 dir = d / len; // arrow direction (unit)

    // --- constant arrowhead size (world units) ---
    const float head_len   = 0.3f; // distance from tip back to where head starts
    const float head_width = 0.2f; // half-width of the V

    // Put head base head_len behind the tip, but don't go past p0 for very short arrows
    float back = std::min(head_len, 0.9f * len);
    glm::vec3 head_base = p1 - dir * back;

    // Choose a reference axis not parallel to dir
    glm::vec3 ref(0, 1, 0);
    if (std::abs(glm::dot(dir, ref)) > 0.99f)
        ref = glm::vec3(1, 0, 0);

    glm::vec3 side = glm::normalize(glm::cross(dir, ref));

    LineInstance left;
    left.p0 = head_base + side * head_width;
    left.p1 = p1;

    LineInstance right;
    right.p0 = head_base - side * head_width;
    right.p1 = p1;

    // return { left, middle, right };
    return { left, middle, right };
}

int xy_id(int x, int y, int ring_width, int cloud_size){
    int idx = x + y * ring_width;

    if (idx < 0 || idx >= cloud_size) {
        throw "Bad index";
        return -1;
    }

    return idx;
}

    float get_color_float(int color_id, int num_colors){ 
    return (float)color_id / (float)(num_colors - 1); 
}

bool is_point_valid(const PointXYZRGBI &p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

float radial_distance(const PointXYZRGBI &p) {
    return std::hypot(static_cast<double>(p.x),
                    static_cast<double>(p.y),
                    static_cast<double>(p.z));
}

float euclid_distance(const PointXYZRGBI &a, const PointXYZRGBI &b) {
    return std::hypot(static_cast<double>(a.x - b.x),
                    static_cast<double>(a.y - b.y),
                    static_cast<double>(a.z - b.z));
}

bool is_same_object(const PointXYZRGBI &p0,
                            const PointXYZRGBI &p1,
                            float rel_thresh = 1.5f,  
                            bool more_permissive_with_distance = true,
                            float abs_thresh = 0.12f)
{
    if (!is_point_valid(p0) || !is_point_valid(p1))
        return false;

    float r0 = radial_distance(p0);
    float r1 = radial_distance(p1);

    if (!std::isfinite(r0) || !std::isfinite(r1))
        return false;

    float allowed = 0;
    float dr = std::fabs(r0 - r1);


    if (more_permissive_with_distance) {
        float thresh = std::max(0.2f - p0.y, 0.0f);
        allowed = std::max(thresh * std::min(r0, r1), abs_thresh);
    }
        // allowed = rel_thresh * pow((std::min(r0, r1) / std::pow(permission_factor, 1.5)), permission_factor);
    else {
        float thresh = std::max(0.2f - p0.y, 0.0f);
        allowed = std::max(thresh, abs_thresh);
        // allowed = std::max(rel_thresh, abs_thresh);
    }
        
    

    return dr <= allowed;
}


glm::vec3 triangle_normal(const PointXYZRGBI& a, const PointXYZRGBI& b, const PointXYZRGBI& c) {
    glm::vec3 av = {a.x, a.y, a.z};
    glm::vec3 bv = {b.x, b.y, b.z};
    glm::vec3 cv = {c.x, c.y, c.z};

    glm::vec3 u = bv - av;
    glm::vec3 v = cv - av;
    glm::vec3 n = glm::cross(u, v);           // unnormalized normal (also proportional to triangle area)
    return glm::normalize(n);       // returns {0,0,0} if degenerate
}


void push_point(std::vector<float>& vertices, const PointXYZRGBI& point, const glm::vec3& color, const glm::vec3& normal) {
        vertices.push_back(point.x);
        vertices.push_back(point.y);
        vertices.push_back(point.z);
        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);
        vertices.push_back(color.x);
        vertices.push_back(color.y);
        vertices.push_back(color.z);
}


Mesh* get_mesh_from_point_cloud(std::vector<PointXYZRGBI> points, float rel_thresh = 1.5f) {
    
    VertexLayout* vertex_layout = new VertexLayout();
    vertex_layout->add("position", 0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 0, 0, {0.0f, 0.0f, 0.0f});
    vertex_layout->add("normal", 1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 3 * sizeof(float), 0, {0.0f, 1.0f, 0.0f});
    vertex_layout->add("color", 2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), 6 * sizeof(float), 0, {1.0f, 1.0f, 1.0f});

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    
    int rings_count = 16;
    int ring_width = points.size() / rings_count;
    int cloud_size = points.size();
    
    for (int y = 0; y < rings_count - 1; y++){
        bool top_jump = false;
        bool bottom_jump = false;
        for (int x = 0; x < ring_width - 1; x++){



            int id1 = xy_id(x, y, ring_width, cloud_size);
            int id2 = xy_id(x, y + 1, ring_width, cloud_size);
            int id3 = xy_id(x + 1, y + 1, ring_width, cloud_size);

            int id4 = xy_id(x + 1, y + 1, ring_width, cloud_size);
            int id5 = xy_id(x + 1, y, ring_width, cloud_size);
            int id6 = xy_id(x, y, ring_width, cloud_size);


            // float top_color = get_color_float(WHITE_ID, num_colors);
            // float bottom_color = get_color_float(WHITE_ID, num_colors);

            //Point positions:
            //1 2
            //0 3

            PointXYZRGBI p0 = points[id1]; // lower-left
            PointXYZRGBI p1 = points[id2]; // upper-left
            PointXYZRGBI p2 = points[id3]; // upper-right
            PointXYZRGBI p3 = points[id5]; // lower-right

            bool tri1_ok = false;
            bool tri2_ok = false;

            if (is_point_valid(p0) && is_point_valid(p1) && is_point_valid(p2)) {
                if (is_same_object(p0, p1, rel_thresh) && is_same_object(p1, p2, 1.5, false))
                    tri1_ok = true;
            }

            if (is_point_valid(p2) && is_point_valid(p3) && is_point_valid(p0)) {
                if (is_same_object(p3, p2, rel_thresh) && is_same_object(p3, p0, 1.5, false))
                    tri2_ok = true;
            }

            glm::vec3 surface_normal = triangle_normal(p2, p0, p1);
            glm::vec3 upward_normal = glm::vec3(0.0f, 1.0f, 0.0f);

            // std::cout << surface_normal.x << " " << surface_normal.y << " " << surface_normal.z << " " << std::endl;

            float normal_cos = glm::dot(surface_normal, upward_normal);

            if (tri1_ok && tri2_ok) {
                // glm::vec3 red = {1.0f, 0.0f, 0.0f};
                glm::vec3 white = {1.0f, 1.0f, 1.0f};
                // float t = glm::clamp(std::abs(normal_cos), 0.0f, 1.0f);
                // glm::vec3 color = red + (white - red) * t;
                glm::vec3 color = white;
                
                int last_id = static_cast<unsigned int>(vertices.size() / 9);
                push_point(vertices, p0, color, surface_normal);
                push_point(vertices, p1, color, surface_normal);
                push_point(vertices, p2, color, surface_normal);
                push_point(vertices, p3, color, surface_normal);

                indices.push_back(last_id);
                indices.push_back(last_id+1);
                indices.push_back(last_id+2);

                indices.push_back(last_id+2);
                indices.push_back(last_id+3);
                indices.push_back(last_id);

            }
        }
    }

    Mesh* mesh = new Mesh(vertices, indices, vertex_layout);

    return mesh;
}


void draw_f(Window* window, 
    Camera* camera, 
    VoxelGrid* voxel_grid, 
    NonholonomicAStar* astar,
    PlainAstarData plain_a_star_path,
    const NonholonomicPos& start_pos, const NonholonomicPos& end_pos, 
    int size) {
    glm::ivec3 camera_voxel_pos = glm::ivec3(glm::floor(camera->position));

    voxel_grid->edit_voxels([&](VoxelEditor& voxel_editor){

        glm::ivec3 left_top_pos = (glm::ivec3)start_pos.pos -  glm::ivec3(size/2.0f, 0, size/2.0f);

        Voxel paint_voxel;

        float max_f = 0;
        for (int x = 0; x < size; x++)
            for (int z = 0; z < size; z++) {
                NonholonomicPos new_pos;
                new_pos.pos = (glm::vec3)left_top_pos + glm::vec3(x, 0, z);

                float f = astar->get_nonholonomic_f(new_pos, end_pos, new_pos, plain_a_star_path);

                if (f > max_f)
                    max_f = f;
            }

        for (int x = 0; x < size; x++)
            for (int z = 0; z < size; z++) {
                NonholonomicPos new_pos;
                new_pos.pos = (glm::vec3)left_top_pos + glm::vec3(x, 0, z);

                float f = astar->get_nonholonomic_f(new_pos, end_pos, new_pos, plain_a_star_path);

                float color_value = f / max_f;

                paint_voxel.color = glm::vec3(0.0, 0.0, color_value);
                paint_voxel.visible = true;

                voxel_editor.set((glm::ivec3)new_pos.pos - glm::ivec3(0, 1, 0), paint_voxel);
            }
    });
}


int main() {
    Engine3D engine = Engine3D();
    Window window = Window(&engine, 1280, 720, "3D visualization");
    engine.set_window(&window);
    ui::init(window.window);
    
    

    // ShaderManager shader_manager = ShaderManager(executable_dir_str());

    Camera camera;
    window.set_camera(&camera);

    FPSCameraController camera_controller = FPSCameraController(&camera);
    camera_controller.speed = 50;

    // VoxelGrid voxel_grid = VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    // float chunk_render_size = voxel_grid.chunk_size.x * voxel_grid.voxel_size;

    // VoxelRasterizatorGPU voxel_rastorizator(&voxel_grid, shader_manager);

    // VtkMeshLoader vtk_mesh_loader = VtkMeshLoader(vertex_layout);

    // MeshData model_mesh_data = vtk_mesh_loader.load_mesh((executable_dir() / "models" / "test_mesh.vtk").string());
    // Mesh model = Mesh(model_mesh_data.vertices, model_mesh_data.indices, &vertex_layout); 
    // model.position = {chunk_render_size * 0, chunk_render_size * 5, chunk_render_size * 0};
    // model.scale = glm::vec3(5.0f);

    // VoxelGridGPU voxel_grid_gpu = VoxelGridGPU(
    //     {16, 16, 16}, // chunk_size
    //     {1.0f, 1.0f, 1.0f}, // voxel_size
    //     100'000, // count_active_chunks
    //     30'000'000, // max_quads
    //     4, // chunk_hash_table_size_factor
    //     512, // max_count_probing
    //     64, // count_evict_buckets
    //     4'000, // min_free_chunks
    //     10'000, // max_evict_chunks
    //     32, // bucket_step
    //     shader_manager
    // );

    // std::vector<glm::ivec3> positions;
    // std::vector<VoxelGridGPU::VoxelDataGPU> voxels;
    // create_occlusion_test_box({0, 0, 0}, positions, voxels);
    // voxel_grid_gpu.apply_writes_to_world_from_cpu(positions, voxels);

    glm::vec3 prev_cam_pos = camera_controller.camera->position;

    float timer = 0;
    float lastFrame = 0;

    VoxelGrid* voxel_grid = new VoxelGrid({16, 16, 16}, 1.0f, {24, 6, 24});
    voxel_grid->update(&window, &camera);
    // sleep(1);

    NonholonomicPos start_pos = NonholonomicPos{glm::ivec3(-0.388, 1.2f, -1.719), 0};
    NonholonomicPos end_pos = NonholonomicPos{glm::ivec3(-4.061, 1.2f, -7.569), 0};

    float const path_lines_width = 5.0f;
    std::vector<Line*> path_lines;
    std::vector<std::vector<LineInstance>> path_line_instances;
    std::vector<std::pair<glm::ivec3, Voxel>> old_voxels;

    Line* start_dir_line = new Line();
    start_dir_line->color = glm::vec3(1.0f, 0.466666667f, 0.0f);
    Line* end_dir_line = new Line();
    end_dir_line->color = glm::vec3(0.023529412f, 0.768627451f, 1.0f);

    glm::vec3 dir(std::cos(end_pos.theta), 0.0f, std::sin(end_pos.theta));
    end_dir_line->set_lines(get_arrow(end_pos.pos, end_pos.pos + dir * 1.0f));

    glm::vec3 dir2(std::cos(start_pos.theta), 0.0f, std::sin(start_pos.theta));
    start_dir_line->set_lines(get_arrow(start_pos.pos, start_pos.pos + dir2 * 1.0f));

    Line* explored_path_lines = new Line();
    explored_path_lines->color = glm::vec3(0.0f, 0, 0);

    NonholonomicAStar* nonholonomic_astar = new NonholonomicAStar(voxel_grid);
    bool simulation_running = false;

    ReedsShepp reeds_shepp;
    float min_radius = 5;
    // std::vector<NonholonomicPathElement> reeds_shepp_test_path = reeds_shepp.get_optimal_path(start_pos, end_pos, min_radius);

    // std::vector<NonholonomicPathElement> reeds_shepp_test_path;
    // std::vector<NonholonomicPathElement> reeds_shepp_test_path = reeds_shepp.path1(end_pos.pos.x, -end_pos.pos.z, -end_pos.theta);
    // reeds_shepp_test_path.push_back(NonholonomicPathElement(5.0f, Steering::STRAIGHT, Gear::FORWARD));
    // reeds_shepp_test_path.push_back(NonholonomicPathElement(5.0f, Steering::STRAIGHT, Gear::FORWARD));
    // reeds_shepp_test_path.push_back(NonholonomicPathElement(2.0f, Steering::LEFT, Gear::BACKWARD));
    // reeds_shepp_test_path.push_back(NonholonomicPathElement(5.0f, Steering::STRAIGHT, Gear::FORWARD));
    // reeds_shepp_test_path.push_back(NonholonomicPathElement(2.0f, Steering::RIGHT, Gear::FORWARD));
    // reeds_shepp_test_path.push_back(NonholonomicPathElement(5.0f, Steering::STRAIGHT, Gear::FORWARD));
    // std::vector<NonholonomicPos> discretized_path = reeds_shepp.discretize_path(reeds_shepp_test_path, 4, min_radius);

    // std::vector<LineInstance> reeds_shepp_test_line_instances;
    // if (discretized_path.size() >= 2)
    // for (int i = 0; i < discretized_path.size() - 1; i++) {
    //     LineInstance line_instance;
    //     line_instance.p0 = discretized_path[i].pos;
    //     line_instance.p1 = discretized_path[i+1].pos;

    //     reeds_shepp_test_line_instances.push_back(line_instance);
    // }
    
    Line* reeds_shepp_test_path_lines = new Line();
    reeds_shepp_test_path_lines->color = glm::vec3(1.0f, 0, 0);
    reeds_shepp_test_path_lines->width = 5;

    Line* unimpended_line = new Line();
    unimpended_line->color = glm::vec3(1.0f, 1.0f, 0);
    unimpended_line->width = 5;


    // end_pos.pos = camera->position;
    // voxel_grid->adjust_to_ground(end_pos.pos);
    // end_pos.pos.y += 0.2f;

    end_pos.theta = glm::radians(30.0f);
    glm::vec3 end_dir(std::cos(end_pos.theta), 0.0f, std::sin(end_pos.theta));
    end_dir_line->set_lines(get_arrow(end_pos.pos, end_pos.pos + end_dir * 1.0f));

    bool force_stop = false;
    bool TEMP_block_start_pathfinnding = false;


    // Frame frame = read_frame_bin("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000067.bin");

    // std::vector<IndexEntry> point_cloud_entries = read_index_csv("/home/spectre/TEMP_lidar_output_mesh/recording/index.csv");


    glm::vec3 origin = glm::vec3(0.0f, 10.0f, 0.0f);

    std::vector<Line*> point_cloud_video_lines;
    std::vector<Point*> point_cloud_video_points;
    std::vector<Mesh*> lidar_meshes;

    PointCloudFrame point_cloud_frame = PointCloudFrame("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");
    PointCloudVideo point_cloud_video = PointCloudVideo();
    point_cloud_video.load_from_file("/home/spectre/TEMP_lidar_output_mesh/recording/index.csv");
    // point_cloud_frame.load_from_file("/home/spectre/TEMP_lidar_output_mesh/recording/frame_000000.bin");

    // for (int i = 0; i < point_cloud_entries.size(); i++) {
    //     std::filesystem::path file_path = "/home/spectre/TEMP_lidar_output_mesh/recording/" / point_cloud_entries[i].filename;
    //     Frame frame = read_frame_bin(file_path);


    //     float minZ = std::numeric_limits<float>::infinity();
    //     float maxZ = -std::numeric_limits<float>::infinity();

    //     for (size_t i = 0; i < frame.points.size(); ++i) {
    //         const auto& p = frame.points[i];
    //         glm::vec3 pos(-p.x, p.z, p.y);
    //         minZ = std::min(minZ, pos.y); // using pos.y as "up" in your convention
    //         maxZ = std::max(maxZ, pos.y);
    //     }

    //     float inv = (maxZ > minZ) ? (1.0f / (maxZ - minZ)) : 0.0f;

    //     std::vector<PointInstance> point_cloud_point_instance;
    //     point_cloud_point_instance.reserve(frame.points.size());
        
    //     for (int i = 0; i < frame.points.size(); i++) {
    //         PointXYZRGBI point = frame.points[i - 1];
    //         glm::vec3 pos = glm::vec3(-point.x, point.z, point.y);

    //         float t = (pos.y - minZ) * inv;        // 0..1
    //         t = clamp01(t);

    //         float hue = (1.0f - t) * 0.66f;
            
    //         PointInstance point_instance;
    //         point_instance.pos = pos;
            
            
    //         point_instance.color = hsv_to_rgb(hue, 1.0f, 1.0f);

    //         point_cloud_point_instance.push_back(point_instance);

    //     }

    //     Point* point = new Point();
    //     point->set_points(point_cloud_point_instance);
    //     point->size_px = 2.0f;

    //     point_cloud_video_points.push_back(point);

    //     std::vector<LineInstance> point_clound_line_instance;
    //     for (int i = 1; i < frame.points.size(); i++) {

    //         PointXYZRGBI point_0 = frame.points[i - 1];
    //         PointXYZRGBI point_1 = frame.points[i];

    //         glm::vec3 pos_0 = glm::vec3(-point_0.x, point_0.z, point_0.y);
    //         glm::vec3 pos_1 = glm::vec3(-point_1.x, point_1.z, point_1.y);

    //         LineInstance line_instance;
    //         line_instance.p0 = pos_0;
    //         line_instance.p1 = pos_1;

    //         point_clound_line_instance.push_back(line_instance);
    //     }
        
    //     Line* point_clound_line = new Line();
    //     point_clound_line->color = {1.0f, 0.0f, 0.0f};
    //     point_clound_line->set_lines(point_clound_line_instance);

    //     point_cloud_video_lines.push_back(point_clound_line);


    //     Mesh* mesh = get_mesh_from_point_cloud(frame.points);
    //     lidar_meshes.push_back(mesh);
    // }

    // std::filesystem::path file_path = "/home/spectre/TEMP_lidar_output_mesh/recording/" / point_cloud_entries[0].filename;
    // Frame frame = read_frame_bin(file_path);
    
    // Mesh* lidar_mesh = get_mesh_from_point_cloud(frame.points);
    
    float rel_thresh = 1.5f;
    // window->disable_cursor();
    while(window.is_open()) {
        float currentFrame = (float)glfwGetTime();
        float delta_time = currentFrame - lastFrame;
        timer += delta_time;
        lastFrame = currentFrame;   

        ui::begin_frame();
        ui::update_mouse_mode(&window);

        camera_controller.update(&window, delta_time);

        window.clear_color({clear_col[0], clear_col[1], clear_col[2], clear_col[3]});

        // voxel_grid->update(&window, &camera);
        // window.draw(voxel_grid, &camera);
        
        ImGui::Begin("Debug");

        glm::vec3 p = camera.position; // or any vec3

        ImGui::TextColored(ImVec4(1,0.5,0.5,1), "x: %.3f", p.x);
        ImGui::TextColored(ImVec4(0.5,1,0.5,1), "y: %.3f", p.y);
        ImGui::TextColored(ImVec4(0.5,0.5,1,1), "z: %.3f", p.z);

        ImGui::SliderFloat("Relative threshold", &rel_thresh, 0.0f, 10.0f);

        ImGui::End();

        ui::end_frame();

        // for (int i = 0; i < path_lines.size(); i++)
        //     window->draw(path_lines[i], camera);
        
        // explored_path_lines->set_lines(nonholonomic_astar->state_explored_paths);
        // window.draw(explored_path_lines, &camera);
            
        // window.draw(start_dir_line, &camera);
        // window.draw(end_dir_line, &camera);
        // window.draw(unimpended_line, &camera);

        // window.draw(reeds_shepp_test_path_lines, &camera);

        // int cur_frame_id = 0;
        // for (int i = 0; i < point_cloud_entries.size(); i++) {
        //     double time_seconds = (double)point_cloud_entries[i].timestamp_ns / 1000000000.0f;

        //     if (time_seconds > video_timer)
        //         break;
        //     else
        //         cur_frame_id = i;
        // }

        // std::cout << cur_frame_id << std::endl;


        // window.draw(point_cloud_video_points[cur_frame_id], &camera);
        // window.draw(lidar_meshes[cur_frame_id], &camera);
        // window.draw(&point_cloud_frame, &camera);
        // point_cloud_frame.point_cloud.position.x += delta_time * 0.2f;
        // point_cloud_frame.point_cloud.rotation.x += delta_time * 180.0f;
        // point_cloud_frame.rotation.y += delta_time * 30.0f;

        point_cloud_video.update(delta_time);
        window.draw(&point_cloud_video, &camera);
        // point_cloud_video.rotation.x += delta_time;

        // Mesh* lidar_mesh = get_mesh_from_point_cloud(frame.points, rel_thresh);
        // window.draw(lidar_mesh, &camera);
        // delete lidar_mesh;
        window.swap_buffers();
        engine.poll_events();
    }
    
    ui::shutdown();
}
