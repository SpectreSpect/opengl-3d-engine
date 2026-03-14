#version 430
layout(local_size_x = 256) in;

#include "utils.glsl"

#define HASH_KEY_TYPE uvec2
#define HASH_VALUE_TYPE vec3
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

HASH_VALUE_TYPE slot_init_func(out bool is_success) {
    // HASH_VALUE_TYPE chunk_id = pop_free_chunk_id();
    // is_success = chunk_id != INVALID_ID;
    return vec3(0, 0, 0);
}

uniform uint u_hash_table_size = 16;



#include "hash_table.glsl"


void main() {
    uint thread_id = gl_GlobalInvocationID.x;

    if (thread_id > 8) return;

    // HASH_KEY_TYPE key, out uint out_slot_id, out bool created
    uint slot_id = 0u;
    bool created = false;
    get_or_create_chunk(uvec2(thread_id, 12u), slot_id, created);

    // // hash_table[0u].hash_key = uvec2(3u, 5u);
    hash_table[slot_id].hash_value = vec3(slot_id, slot_id + 1, slot_id + 2);
    // hash_table[thread_id].hash_value = vec3(1, 2, 3);
    
    // if (chunkId >= u_max_chunks) return;
}
