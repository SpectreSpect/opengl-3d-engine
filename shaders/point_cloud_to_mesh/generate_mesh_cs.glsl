#version 430
layout(local_size_x = 256) in;



struct Point {
    vec4 pos;
    vec4 color;
};
layout(std430, binding=0) readonly buffer PointCloud { Point cloud_points[]; };

struct Vertex {
    vec4 position;
    vec4 normal;
    vec4 color;
};
layout(std430, binding=1) buffer VertciesOut { Vertex vertices_out[]; };
layout(std430, binding=2) buffer IndicesOut { uint indices_out[]; };

uniform uint triangles_count;

uint xy_id(uint x, uint y, uint ring_width) {
    uint idx = x + y * ring_width;

    // if (idx < 0 || idx >= cloud_size) {
    //     return -1;
    // }

    return idx;
}

vec4 triangle_normal(vec4 a, vec4 b, vec4 c) {
    vec3 av = vec3(a.x, a.y, a.z);
    vec3 bv = vec3(b.x, b.y, b.z);
    vec3 cv = vec3(c.x, c.y, c.z);

    vec3 u = bv - av;
    vec3 v = cv - av;
    vec3 n = cross(u, v);           // unnormalized normal (also proportional to triangle area)
    return vec4(normalize(n), 1.0);       // returns {0,0,0} if degenerate
}

bool is_point_valid(vec4 p) {
    // return 
    //     !(isnan(p.x) || isnan(p.y) || isnan(p.z)) &&
    //     !(isinf(p.x) || isinf(p.y) || isinf(p.z));

    if (p.x > 100 || p.y > 100 || p.z > 100)
        return false;
    if (p.x < -100 || p.y < -2 || p.z < -100)
        return false;
    return true;
}

float radial_distance(vec4 p)
{
    return length(p.xyz);
}


bool is_same_object(vec4 p0, vec4 p1, float rel_thresh, bool more_permissive_with_distance, float abs_thresh)
{
    if (!is_point_valid(p0) || !is_point_valid(p1))
        return false;

    float r0 = radial_distance(p0);
    float r1 = radial_distance(p1);

    if (isnan(r0) || isinf(r0) || isnan(r1) || isinf(r1))
        return false;

    float allowed = 0;
    float dr = abs(r0 - r1);


    if (more_permissive_with_distance) {
        // float thresh = max(0.2 - p0.y, 0.0);
        // allowed = max(thresh * min(r0, r1), abs_thresh);
        float permission_factor = 1.5;
        allowed = rel_thresh * pow((min(r0, r1) / pow(permission_factor, 1.5)), permission_factor);
    }
    else {
        // float thresh = max(0.2 - p0.y, 0.0);
        // allowed = max(thresh, abs_thresh);
        allowed = max(rel_thresh, abs_thresh);
    }
    return dr <= allowed;
}


void main(){
    uint triangle_id = gl_GlobalInvocationID.x;
    uint ring_id = gl_GlobalInvocationID.y;

    if (triangle_id >= triangles_count) return;

    uint first_vertex_id = triangle_id * 3 + ring_id * triangles_count * 3;
    uint left_bottom_point_id = triangle_id / 2;
    bool is_triangle_0 = triangle_id % 2 == 0;
    
    uint ring_size = triangles_count / 2;

    Point p0 = cloud_points[xy_id(left_bottom_point_id, ring_id, ring_size)];
    Point p1 = cloud_points[xy_id(left_bottom_point_id, ring_id+1, ring_size)];
    Point p2 = cloud_points[xy_id(left_bottom_point_id+1, ring_id+1, ring_size)];
    Point p3 = cloud_points[xy_id(left_bottom_point_id+1, ring_id, ring_size)];

    vec4 normal = vec4(0.0);

    float rel_thresh = 1.5;
    float abs_thresh = 0.12;

    if (is_triangle_0) {
        bool ok = false;
        if (is_point_valid(p0.pos) && is_point_valid(p1.pos) && is_point_valid(p1.pos)) {
            if (is_same_object(p0.pos, p1.pos, rel_thresh, true, abs_thresh) && is_same_object(p1.pos, p2.pos, rel_thresh, false, abs_thresh))
                ok = true;
        }

        if (!ok)
            return;
        
        vertices_out[first_vertex_id].position = p0.pos;
        vertices_out[first_vertex_id+1].position = p1.pos;
        vertices_out[first_vertex_id+2].position = p2.pos;

        normal = triangle_normal(p0.pos, p1.pos, p2.pos);
    }
    else {
        bool ok = false;
        if (is_point_valid(p2.pos) && is_point_valid(p3.pos) && is_point_valid(p0.pos)) {
            if (is_same_object(p3.pos, p2.pos, rel_thresh, true, abs_thresh) && is_same_object(p3.pos, p0.pos, rel_thresh, false, abs_thresh))
                ok = true;
        }

        if (!ok)
            return;
        
        vertices_out[first_vertex_id].position = p2.pos;
        vertices_out[first_vertex_id+1].position = p3.pos;
        vertices_out[first_vertex_id+2].position = p0.pos;

        normal = triangle_normal(p2.pos, p3.pos, p0.pos);
    }
    
    vec4 color = vec4(1, 0, 1, 1.0);
    // if (is_triangle_0)
    //     color = vec4(0, 0, 0, 1);
    if (ring_id % 2 == 0) {
        if (left_bottom_point_id % 10 >= 5)
            color = vec4(0, 0, 0, 1); 
    } else {
        if (left_bottom_point_id % 10 < 5)
            color = vec4(0, 0, 0, 1); 
    }
        
    
    // if (ring_id >= 10 && ring_id <= 13 && left_bottom_point_id >= 500 && left_bottom_point_id <= 700)
    if (ring_id >= 9 && ring_id <= 10 && left_bottom_point_id >= 1600 && left_bottom_point_id <= 1800)
        color = vec4(1, 0, 0, 1);


    for (uint i = 0; i < 3; i++) {
        uint vertex_id = first_vertex_id + i;
        vertices_out[vertex_id].normal = normal;
        vertices_out[vertex_id].color = color;
    }

    indices_out[first_vertex_id] = first_vertex_id;
    indices_out[first_vertex_id+1] = first_vertex_id+1;
    indices_out[first_vertex_id+2] = first_vertex_id+2;
}
