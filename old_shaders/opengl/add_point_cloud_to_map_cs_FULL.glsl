#version 430
layout(local_size_x = 256) in;


#define CAT_(A, B) A##B
#define CAT(A, B)  CAT_(A, B)
#define CAT3_(A, B, C) A##B##C
#define CAT3(A, B, C)  CAT3_(A, B, C)
#define PR(PREFIX, NAME) CAT3(PREFIX, _, NAME)
#define SF(NAME, SUFFIX) CAT3(NAME, _, SUFFIX)

#define INVALID_ID 0xFFFFFFFFu
#define BYTE_MASK 0xFFu

uint max(uint a, uint b) {
    return a > b ? a : b;
}

uint div_up_u32(uint a, uint b) { 
    return (a + b - 1u) / b; 
}

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    return uint(findMSB(x - 1u) + 1);
}

int floor_div(int a, int d) {
    int q = a / d;
    int r = a - q * d;
    if (r != 0 && a < 0) q -= 1;
    return q;
}

int floor_mod(int a, int d) {
    int q = floor_div(a, d);
    return a - q * d; // [0..d-1]
}

uint mask_bits(uint bits) { 
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u); 
}

uint hash_u32(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float hash_ivec2(ivec2 p, uint seed) {
    uint h = uint(p.x) * 374761393u
           + uint(p.y) * 668265263u
           + seed     * 2246822519u;

    h = hash_u32(h);
    return float(h) * (1.0 / 4294967295.0);
}

uvec2 pack_key_uvec2(ivec3 c, uint pack_offset, uint pack_bits) {
    uint B = pack_bits;
    uint m = mask_bits(B);

    uint ux = uint(c.x + pack_offset) & m;
    uint uy = uint(c.y + pack_offset) & m;
    uint uz = uint(c.z + pack_offset) & m;

    uint lo = 0u, hi = 0u;
    lo |= uz;

    // uy at shift B
    if (B < 32u) {
        lo |= (uy << B);
        if (B != 0u && (B + B) > 32u) hi |= (uy >> (32u - B));
    } else {
        hi |= (uy << (B - 32u));
    }

    // ux at shift 2B
    uint s = 2u * B;
    if (s < 32u) {
        lo |= (ux << s);
        if (B != 0u && (s + B) > 32u) hi |= (ux >> (32u - s));
    } else if (s == 32u) {
        hi |= ux;
    } else {
        hi |= (ux << (s - 32u));
    }

    return uvec2(lo, hi);
}

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint res = lo_part;
        uint rem = 32u - shift;
        if (rem < bits) res |= (k.y << rem);
        return res & m;
    } else {
        uint s = shift - 32u;
        return (k.y >> s) & m;
    }
}

ivec3 unpack_key_to_coord(uvec2 key2, uint pack_offset, uint pack_bits) {
    uint B = pack_bits;
    uint ux = extract_bits_uvec2(key2, 2u * B, B);
    uint uy = extract_bits_uvec2(key2, 1u * B, B);
    uint uz = extract_bits_uvec2(key2, 0u * B, B);

    return ivec3(int(ux) - pack_offset,
                 int(uy) - pack_offset,
                 int(uz) - pack_offset);
}

uint pack_color(vec4 rgba) {
    rgba = clamp(rgba, 0.0, 1.0);
    uint r = uint(rgba.r * 255.0 + 0.5);
    uint g = uint(rgba.g * 255.0 + 0.5);
    uint b = uint(rgba.b * 255.0 + 0.5);
    uint a = uint(rgba.a * 255.0 + 0.5);
    return (r << 24) | (g<<16) | (b<<8) | a;
}

uint pack_color(vec3 rgb) {
    return pack_color(vec4(rgb, 1u));
}

struct PointInstance {
    vec4 position;
    vec4 color;
};

struct PointNormal {
    vec4 normal;
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

layout(std430, binding=0) coherent buffer HashTable {uint count_tomb; HashTableSlot hash_table[]; }; 
layout(std430, binding=1) coherent buffer MapPoints {PointInstance map_points[]; }; 
layout(std430, binding=2) coherent buffer MapNormals {PointNormal map_normals[];}; 
layout(std430, binding=3) coherent buffer NumPoints {uint num_map_points; }; 

layout(std430, binding=4) coherent buffer SourcePoints {PointInstance source_points[];}; 
layout(std430, binding=5) coherent buffer SourceNormals {PointNormal source_normals[];}; 

HASH_VALUE_TYPE slot_init_func(out bool is_success) {
    return 0;
}

uniform uint u_hash_table_size = 16;




/*
Для настройки #include можно определить следующие дефайны (не обязательно):
#define TOMB_CHECK_LIST_SIZE n (число n должно быть n = 2^i)
#define NOT_INCLUDE_GET_OR_CREATE (позволяет не подключить get_or_create_chunk())
#define NOT_INCLUDE_LOOKUP_REMOVE (позволяет не подключать lookup_chunk() и remove_from_table())

Также необходимо наличие следующих данных (названия обязательно должны соответсвовать указанным).

Буфферы:

coherent buffer HashKeys { uvec2 hash_keys[]; };
coherent buffer HashVals { uint count_tomb; uint  hash_vals[]; };

coherent buffer HashTable {uint count_tomb; HashTableSlot hash_table[]; }; 

buffer FreeList { uint free_list[]; }; (только если подключён get_or_create_chunk())
buffer ChunkMetaBuf { ChunkMeta meta[]; }; (только если подключён get_or_create_chunk())

HASH_KEY_TYPE
HASH_VALUE_TYPE

Юниформы:
uniform uint u_hash_table_size;
*/

// HASH_VALUE_TYPE slot_init_func(out bool is_success) {
//     HASH_VALUE_TYPE chunk_id = pop_free_chunk_id();
//     is_success = chunk_id != INVALID_ID;
//     return chunk_id;
// }

// #define HASH_KEY_TYPE uvec2
// #define HASH_VALUE_TYPE uint
// #define HASH_TABLE hash_table
// #define HASH_TABLE_COUNT_TOMB count_tomb
// #define HASH_TABLE_SLOT_INIT_FUNC slot_init_func
// #define KEY_HASH_FUNC hash_uvec2
// #define HASH_TABLE_SIZE u_hash_table_size

// struct HashTableSlot {
//     HASH_KEY_TYPE hash_key;
//     HASH_VALUE_TYPE hash_value;
//     uint hash_state;
// };

#ifndef HASH_TABLE_COMMON
#define HASH_TABLE_COMMON

#define SLOT_EMPTY    0u
#define SLOT_LOCKED   1u
#define SLOT_TOMB     2u
#define SLOT_OCCUPIED 3u
#define MAX_PROBES   128u

#ifndef TOMB_CHECK_LIST_SIZE
#define TOMB_CHECK_LIST_SIZE 32u
#endif

uint TOMB_LIST_MASK = TOMB_CHECK_LIST_SIZE - 1u;

uint tomb_check_list[TOMB_CHECK_LIST_SIZE]; // НЕ ЗАБЫТЬ СДЕЛАТЬ УНИКАЛЬНЫМ!!!
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
#endif

#ifndef NOT_INCLUDE_GET_OR_CREATE
#ifndef HASH_TABLE_GET_OR_CREATE
#define HASH_TABLE_GET_OR_CREATE

// uint pop_free_chunk_id() {
//     for (;;) {
//         uint old_counter = atomicAdd(free_count, 0u);
//         if (old_counter == 0u) return INVALID_ID;
        
//         if (atomicCompSwap(free_count, old_counter, old_counter - 1u) == old_counter) {
//             memoryBarrierBuffer();
//             return free_list[old_counter - 1u];
//         }
//     }
// }

bool get_or_create_chunk(HASH_KEY_TYPE key, out uint out_slot_id, out bool created) {
    uint idx  = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;
    uint last_tomb_id = INVALID_ID;

    tomb_list_head_id = INVALID_ID;
    tomb_list_tail_id = INVALID_ID;
    tomb_list_count_elements = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint state = atomicAdd(HASH_TABLE[idx].hash_state, 0u);

        if (state == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

            uint idx_to_create = last_tomb_id == INVALID_ID ? idx : last_tomb_id;
            uint slot_state = last_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

            if (idx_to_create == INVALID_ID) return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди

            uint prev = atomicCompSwap(HASH_TABLE[idx_to_create].hash_state, slot_state, SLOT_LOCKED);
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

            HASH_VALUE_TYPE value;
            #ifdef HASH_TABLE_SLOT_INIT_FUNC
            bool is_success;
            value = HASH_TABLE_SLOT_INIT_FUNC(is_success);
            if (!is_success) {
                atomicExchange(HASH_TABLE[idx_to_create].hash_state, slot_state);
                return false;
            }
            #endif

            if (slot_state == SLOT_TOMB) {
                atomicAdd(HASH_TABLE_COUNT_TOMB, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как count_tomb > 0)
            }

            // публикуем key
            HASH_TABLE[idx_to_create].hash_key = key;
            HASH_TABLE[idx_to_create].hash_value = value;

            // гарантируем, что key/meta видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            atomicExchange(HASH_TABLE[idx_to_create].hash_state, SLOT_OCCUPIED);

            out_slot_id = idx_to_create;
            created = true;
            return true;
        }

        if (state == SLOT_LOCKED) continue;

        if (state == SLOT_TOMB) {
            push_tomb_id(idx);
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        memoryBarrierBuffer();

        if (HASH_TABLE[idx].hash_key == key) {
            out_slot_id = idx;
            created = false;
            return true;
        }
        
        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    return false;
}
#endif
#endif

#ifndef NOT_INCLUDE_LOOKUP_REMOVE
#ifndef HASH_TABLE_LOOKUP_REMOVE_CHUNK
#define HASH_TABLE_LOOKUP_REMOVE_CHUNK
uint lookup_chunk(HASH_KEY_TYPE key) {
    // uint mask = u_hash_table_size - 1u;
    uint idx  = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint v = atomicAdd(HASH_TABLE[idx].hash_state, 0u);

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        if (v == SLOT_EMPTY) return INVALID_ID;

        // Если мы сюда дошли, значит в слоте стоит чья-то запись. Нужно прочитать ключ
        // Но чтобы прочитать ключ необходимо тоже залочить! (иначе во время прочтения его состояние может уже измениться) 
        memoryBarrierBuffer();

        
        if (all(equal(HASH_TABLE[idx].hash_key, key))) 
            return v;
        
        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    return INVALID_ID;
}

bool remove_from_table(HASH_KEY_TYPE key) {
    // uint mask = HASH_TABLE_SIZE - 1u;
    uint idx  = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint v = atomicAdd(HASH_TABLE[idx].hash_state, 0u);

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_EMPTY) return false;

        if (v == SLOT_TOMB) { 
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        memoryBarrierBuffer();

        if (all(equal(HASH_TABLE[idx].hash_key, key))) {
            atomicExchange(HASH_TABLE[idx].hash_state, SLOT_TOMB); // Удаляем слот
            atomicAdd(HASH_TABLE_COUNT_TOMB, 1u);
            return true;
        }

        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }
    return false;
}

bool set_chunk(HASH_KEY_TYPE key, HASH_VALUE_TYPE chunk_id) {
    // uint mask = u_hash_table_size - 1u;
    uint idx  = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;
    uint last_tomb_id = INVALID_ID;

    tomb_list_head_id = INVALID_ID;
    tomb_list_tail_id = INVALID_ID;
    tomb_list_count_elements = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint v = atomicAdd(HASH_TABLE[idx].hash_state, 0u);
        
        if (v == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

            uint idx_to_create = last_tomb_id == INVALID_ID ? idx : last_tomb_id;
            uint slot_state = last_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

            if (idx_to_create == INVALID_ID) return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди

            
            uint prev = atomicCompSwap(HASH_TABLE[idx_to_create].hash_state, slot_state, SLOT_LOCKED);
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

            if (slot_state == SLOT_TOMB) {
                atomicAdd(HASH_TABLE_COUNT_TOMB, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как count_tomb > 0)
            }

            HASH_TABLE[idx_to_create].hash_key = key;

            // публикуем key
            // hash_keys[idx_to_create] = key;

            // гарантируем, что key/meta видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            // atomicExchange(HASH_TABLE[idx_to_create].hash_state, chunk_id);
            atomicExchange(HASH_TABLE[idx_to_create].hash_state, SLOT_OCCUPIED);
            

            return true;
        }

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            push_tomb_id(idx);
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        if (all(equal(HASH_TABLE[idx].hash_key, key))) {
            return false;
        }

        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    return false;
}
#endif
#endif

#undef HASH_KEY_TYPE
#undef HASH_VALUE_TYPE
#undef HASH_TABLE
#undef HASH_TABLE_COUNT_TOMB
#undef HASH_TABLE_SLOT_INIT_FUNC
#undef KEY_HASH_FUNC
#undef HASH_TABLE_SIZE


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
    
    const uint pack_bits   = 21u;
    const uint pack_offset = 1048575u;

    float voxel_size = 1.0f;
    
    // PointInstance point;
    // point.position = vec4(thread_id, 0, 0, 1);
    

    
    // point.position = vec4(point_id, 3, 0, 0);
    // point.color = vec4(5, 5, 5, 5);
    // map_points[point_id] = source_points[source_point_id];
    vec3 source_pos = source_points[source_point_id].position.xyz;
    
    ivec3 voxel_coord = ivec3(floor(source_pos / voxel_size));

    uvec2 key = pack_key_uvec2(voxel_coord, pack_offset, pack_bits);

    uint slot_id = 0u;
    bool created = false;
    get_or_create_chunk(key, slot_id, created);

    uint map_point_id = 0;
    if (created) {
        map_point_id = atomicAdd(num_map_points, 1u);
        hash_table[slot_id].hash_value = map_point_id;
    } else {
        map_point_id = hash_table[slot_id].hash_value;
    }

    map_points[map_point_id] = source_points[source_point_id];
    map_normals[map_point_id] = source_normals[source_point_id];

    // hash_table[slot_id].hash_value = point_id;
}
