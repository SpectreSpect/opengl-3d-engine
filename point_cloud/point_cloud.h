#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <vector>
#include <fstream>
#include "../transformable.h"
#include "../drawable.h"
#include "../point.h"
#include "../mesh.h"
#include "../compute_program.h"
#include "../compute_shader.h"
#include "../path_utils.h"
#include "../ssbo.h"
#include "../math_utils.h"



class PointCloud : public Drawable, public Transformable{
public:
    struct Vertex {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec4 color;
    };


    ComputeShader mesh_generation_shader;
    ComputeProgram mesh_generation_program;
    Point point_renderer;

    SSBO* point_cloud_ssbo;
    SSBO* vertex_ssbo;
    SSBO* index_ssbo;

    PointCloud();
    void update_points(std::vector<PointInstance> points);
    void set_point(unsigned int id, PointInstance value);
    PointInstance get_point(unsigned int id);
    size_t size();
    void push_point(std::vector<float>& vertices, const PointInstance& point, const glm::vec3& color, const glm::vec3& normal);
    bool is_point_valid(const PointInstance &p);
    float radial_distance(const PointInstance &p);
    bool is_same_object(const PointInstance &p0,const PointInstance &p1,
                        float rel_thresh = 1.5f, bool more_permissive_with_distance = true,
                        float abs_thresh = 0.12f);
    glm::vec3 triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c);
    int xy_id(int x, int y, int ring_width, int cloud_size);
    Mesh generate_mesh(float rel_thresh = 1.5f);
    // void generate_mesh_gpu(const SSBO& point_cloud_ssbo, const SSBO& vertex_ssbo, const SSBO& index_ssbo,  float rel_thresh = 1.5f);
    Mesh generate_mesh_gpu(unsigned int rings_count, unsigned int ring_size,  float rel_thresh = 1.5f);
    void generate_mesh_gpu(const SSBO& point_cloud_ssbo, const SSBO& vertex_ssbo, const SSBO& index_ssbo, 
                           unsigned int rings_count, unsigned int ring_size,  float rel_thresh = 1.5f);
    void print_vertex(const Vertex& vertex);
    void draw(RenderState state) override;
        
private:
    bool needs_gpu_sync = true;
    std::vector<PointInstance> points;
    

    void sync_gpu();
};