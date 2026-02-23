#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer PointCloud { vec4 cloud_points[]; };

struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 color;
};
layout(std430, binding=1) buffer VertciesOut { Vertex vertices_out[]; };
layout(std430, binding=2) buffer IndicesOut { uint indices_out[]; };

uniform uint triangles_count;

void main(){
    uint triangle_id = gl_GlobalInvocationID.x;
    uint ring_id = gl_GlobalInvocationID.y;

    if (triangle_id >= triangles_count) return;

    uint first_vertex_id = triangle_id * 3 + ring_id * triangles_count;

    // if (first_vertex_id >= 3)
    //     return;

    // vertices_out[0].position.x = first_vertex_id;

    vertices_out[first_vertex_id].position = vec4(first_vertex_id, 0.0, 0.0, 1.0);
    vertices_out[first_vertex_id+1].position = vec4(first_vertex_id, 0.0, 5.0, 1.0);
    vertices_out[first_vertex_id+2].position = vec4(first_vertex_id + 5, 0.0, 5.0, 1.0);

    // if (triangle_id == 0 && ring_id == 0) {
    //     vertices_out[1].position = vec4(triangles_count, 5.0, 5.0, 1.0);
    //     // vertices_out[0] = 15;
    //     // vertices_out[3] = 17;
    // }

    // if (triangle_id == 1 && ring_id == 0) {
    //     vertices_out[1].position = vec4(0.0, 0.0, 5.0, 1.0);
    // }

    // if (triangle_id == 2 && ring_id == 0) {
    //     vertices_out[2].position = vec4(5.0, 0.0, 5.0, 1.0);
    // }
    

    vec4 normal = vec4(0, 1, 0, 1.0);
    vec4 color = vec4(1, 1, 1, 1.0);

    for (uint i = 0; i < 3; i++) {
        uint vertex_id = first_vertex_id + i;
        vertices_out[vertex_id].normal = normal;
        vertices_out[vertex_id].color = color;
    }

    uint first_index_id = first_vertex_id;
    indices_out[first_index_id] = first_vertex_id;
    indices_out[first_index_id+1] = first_vertex_id+1;
    indices_out[first_index_id+2] = first_vertex_id+2;
}
