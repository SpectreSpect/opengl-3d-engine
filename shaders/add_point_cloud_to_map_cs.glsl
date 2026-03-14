#version 430
layout(local_size_x = 256) in;

#include "utils.glsl"

struct PointInstance {
    vec4 position;
    vec4 color;
};


#define HASH_KEY_TYPE uvec2
#define HASH_VALUE_TYPE uint
#define HASH_TABLE hash_table
#define HASH_TABLE_COUNT_TOMB count_tomb
#define HASH_TABLE_SLOT_INIT_FUNC slot_init_func
#define KEY_HASH_FUNC hash_uvec2
#define HASH_TABLE_SIZE u_hash_table_size

struct HashTableSlot {
    HASH_KEY_TYPE hash_key;
    HASH_VALUE_TYPE hash_value;
    uint hash_state;
};

// layout(std430, binding=0) coherent buffer HashTable {uint count_tomb; HashTableSlot hash_table[]; }; 
layout(std430, binding=0) coherent buffer HashTable {uint count_tomb; HashTableSlot hash_table[]; }; 
// layout(std430, binding=1) coherent buffer Points {uint num_points; PointInstance points[]; }; 
layout(std430, binding=1) coherent buffer MapPoints {PointInstance map_points[]; }; 
layout(std430, binding=2) coherent buffer NumPoints {uint num_map_points; }; 
layout(std430, binding=3) coherent buffer SourcePoints {PointInstance source_points[];}; 



HASH_VALUE_TYPE slot_init_func(out bool is_success) {
    // HASH_VALUE_TYPE chunk_id = pop_free_chunk_id();
    // is_success = chunk_id != INVALID_ID;
    // PointInstance new_hash_point;

    // new_hash_point.position = vec4(0, 0, 0, 0);
    // new_hash_point.normal = vec4(0, 0, 0, 0);

    return 0;
}

uniform uint u_hash_table_size = 16;



#include "hash_table.glsl"


// bool next_pos(ivec3 origin, int layer, out ivec3 index_pos, out ivec3 out_pos) {
//     float dim_size = 1 + layer * 2;

//     int x = index_pos.x;
//     int y = index_pos.y;
//     int z = index_pos.z;
    
//     if ((z == 0 || z == (dim_size - 1)) || (y == 0 || y == (dim_size - 1))) {
//         x++;
//     }
//     else {
//         if (x == dim_size - 1)
//             x++;
//         else
//             x = dim_size - 1;
//     }

//     if (x >= dim_size) {
//         x = 0;
//         if (z > dim_size)
//             z = 0;
//         else 
//             z++;

//         if (z >= dim_size) {
//             z = 0;
//             y++;

//             if (y >= dim_size)
//                 return true;
//         }
            
//         else 
//             z++;
//     }

//     out_pos = origin + ivec3(x, y, z) - layer;

//     return false;
// }


uniform uint num_source_points;


void main() {
    uint source_point_id = gl_GlobalInvocationID.x;


    if (source_point_id > num_source_points) 
        return;
    
    // PointInstance point;
    // point.position = vec4(thread_id, 0, 0, 1);
    

    uint point_id = atomicAdd(num_map_points, 1u);
    // point.position = vec4(point_id, 3, 0, 0);
    // point.color = vec4(5, 5, 5, 5);
    map_points[point_id] = source_points[source_point_id];

    // uint slot_id = 0u;
    // bool created = false;
    // get_or_create_chunk(uvec2(point_id, 12u), slot_id, created);

    // hash_table[slot_id].hash_value = point_id;
}
