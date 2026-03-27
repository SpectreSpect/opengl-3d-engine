#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer CounterHashTable { HashTableCounters counter_hash_table_counters; CounterHashTableSlot counter_hash_table_slots[]; };
layout(std430, binding=1) buffer TriangleIndicesList { uint triangle_counter; uint triangle_indices_list[]; };

uniform uint u_counter_hash_table_size;

void main() {
    uint slot_id = gl_GlobalInvocationID.x;
    if (slot_id >= u_counter_hash_table_size) return;

    uint base = atomicAdd(triangle_counter, counter_hash_table_slots[slot_id].value.count_triangles);
    counter_hash_table_slots[slot_id].value.triangle_indices_base = base;
    counter_hash_table_slots[slot_id].value.triangle_emmit_counter = base;
}