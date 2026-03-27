#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer CounterHashTable { HashTableCounters counter_hash_table_counters; ChunkHashTableSlot counter_hash_table_slots[]; };
layout(std430, binding=1) buffer CountersList { uint counter[]; };

uniform uint u_counter_hash_table_size;

void main() {
    uint slot_id = gl_GlobalInvocationID.x;
    if (slot_id >= u_counter_hash_table_size) return;

    counter[slot_id] = counter_hash_table_slots[slot_id].value;
}
