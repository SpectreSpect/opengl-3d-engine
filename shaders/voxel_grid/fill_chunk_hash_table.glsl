#version 430
layout(local_size_x = 256) in;

// --- hash table ---
layout(std430, binding=0) coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
layout(std430, binding=1) coherent buffer ChunkHashVals { uint  hash_vals[]; };

// --- chunk meta ---
struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=2) buffer ChunkMetaBuf { ChunkMeta meta[]; };

layout(std430, binding=3) buffer EnqueuedBuf { uint enqueued[]; };

struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=4) buffer FrameCountersBuf { FrameCounters counters; }; // w = freeCount

uniform uint u_max_chunks;
uniform uint u_hash_table_size;
uniform uint u_pack_bits;
uniform int u_pack_offset;

// --- constants ---
#define INVALID_ID   0xFFFFFFFFu

#define SLOT_EMPTY   0xFFFFFFFFu
#define SLOT_LOCKED  0xFFFFFFFEu
#define SLOT_TOMB    0xFFFFFFFDu

#define MAX_PROBES   128u

#define TOMB_CHECK_LIST_SIZE 32u
const uint TOMB_LIST_MASK = TOMB_CHECK_LIST_SIZE - 1u;

// --- variables ---
uint tomb_check_list[TOMB_CHECK_LIST_SIZE];
uint tomb_list_head_id = INVALID_ID;
uint tomb_list_tail_id = INVALID_ID;
uint tomb_list_count_elements = 0u;

bool push_tomb_id(uint slot_id) {
    if (tomb_list_count_elements >= TOMB_CHECK_LIST_SIZE)
        return false;

    if (tomb_list_count_elements == 0u) {
        tomb_list_head_id = 0u;
        tomb_list_tail_id = 0u;
    }
    else  {
        tomb_list_head_id = (tomb_list_head_id + 1u) & TOMB_LIST_MASK;
     }

    tomb_check_list[tomb_list_head_id] = slot_id;
    tomb_list_count_elements++;
    return true;
}

uint pop_tail_tomb_id() {
    if (tomb_list_count_elements == 0u)
        return INVALID_ID;

    uint result = tomb_check_list[tomb_list_tail_id];
    tomb_list_count_elements--;

    if (tomb_list_count_elements == 0u) {
        // Когда элементов нет, не может существовать id элемента головы и хвоста - это логически корректно.
        // В теории можно было бы обойтись другими путями, но кажется так всех проще и логичнее.
        tomb_list_head_id = INVALID_ID;
        tomb_list_tail_id = INVALID_ID;
    } else {
        tomb_list_tail_id = (tomb_list_tail_id + 1u) & TOMB_LIST_MASK;
    }

    return result;
}

// ---------------- hash ----------------
uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

bool set_chunk(uvec2 key, uint chunk_id) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;
    uint last_tomb_id = INVALID_ID;

    tomb_list_head_id = INVALID_ID;
    tomb_list_tail_id = INVALID_ID;
    tomb_list_count_elements = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

            uint idx_to_create = last_tomb_id == INVALID_ID ? idx : last_tomb_id;
            uint slot_state = last_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

            if (idx_to_create == INVALID_ID) return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди

            uint prev = atomicCompSwap(hash_vals[idx_to_create], slot_state, SLOT_LOCKED);
            if (prev != slot_state) {
                // кто-то успел — перепроверяем этот же idx

                if (last_tomb_id != INVALID_ID && prev != SLOT_LOCKED) {
                    // Условие говорит, что мы проверяем tomb_check_list_counter - 1u слот и он оказался занят
                    last_tomb_id = INVALID_ID;
                    continue;
                }
                else {
                    continue;
                }
            }

            // публикуем key
            hash_keys[idx_to_create] = key;

            // гарантируем, что key/meta видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            atomicExchange(hash_vals[idx_to_create], chunk_id);

            return true;
        }

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            push_tomb_id(idx);
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        // Если мы сюда дошли, значит в слоте стоит чья-то запись. Нужно прочитать ключ
        // Но чтобы прочитать ключ необходимо тоже залочить! (иначе во время прочтения его состояние может уже измениться) 

        if (atomicCompSwap(hash_vals[idx], v, SLOT_LOCKED) == v) {
            // Залочили слот - можем читать ключ
            if (all(equal(hash_keys[idx], key))) {
                atomicExchange(hash_vals[idx], v); // Убираем блокировку
                return false;
            }

            atomicExchange(hash_vals[idx], v); // Убираем блокировку
        } else {
            continue; // Не получилось захватить. Попробуем ещё раз.
        }
        
        idx = (idx + 1u) & mask;
        probe++;
    }

    return false;
}

void main() {
    uint chunk_id = gl_GlobalInvocationID.x;
    if (chunk_id >= u_max_chunks) return;

    ChunkMeta chunk_meta = meta[chunk_id];
    if (chunk_meta.used == 0u) return;

    uvec2 key = uvec2(chunk_meta.key_lo, chunk_meta.key_hi);

    set_chunk(key, chunk_id);
}