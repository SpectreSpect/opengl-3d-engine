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

struct HAndG {
    double h[6][6];
    double g[6];
    uint status; // lock flag
};

struct Location {
    vec4 position;
    vec4 rotation;
};

struct TestOutput {
    vec4 output_vector;
};




// mat3 euler_xyz_to_mat3(vec3 euler)

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

layout(std430, binding=6) coherent buffer SharedHAndG {HAndG h_and_g;};

layout(std430, binding=7) coherent buffer PointCloudLocation {Location source_location;};

layout(std430, binding=9) coherent buffer TestOutputTemp {TestOutput test_output;};





uniform uint num_source_points;

HASH_VALUE_TYPE slot_init_func(out bool is_success) {
    return 0;
}

uniform uint u_hash_table_size;




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

mat3 euler_xyz_to_mat3(vec3 euler)
{
    float cx = cos(euler.x);
    float sx = sin(euler.x);

    float cy = cos(euler.y);
    float sy = sin(euler.y);

    float cz = cos(euler.z);
    float sz = sin(euler.z);

    mat3 rx = mat3(
        1.0, 0.0, 0.0,
        0.0,  cx,  sx,
        0.0, -sx,  cx
    );

    mat3 ry = mat3(
         cy, 0.0, -sy,
         0.0, 1.0,  0.0,
         sy, 0.0,  cy
    );

    mat3 rz = mat3(
         cz,  sz, 0.0,
        -sz,  cz, 0.0,
         0.0, 0.0, 1.0
    );

    return rx * ry * rz;
}

vec3 safe_normalize(vec3 v)
{
    float len2 = dot(v, v);
    if (len2 < 1e-12)
        return vec3(0.0);
    return v * inversesqrt(len2);
}

vec3 transform_normal_world(vec3 cloud_rotation,
                            vec3 cloud_scale,
                            vec3 local_n)
{
    if (dot(local_n, local_n) < 1e-12)
        return vec3(0.0);

    mat3 R = euler_xyz_to_mat3(cloud_rotation);

    mat3 S = mat3(1.0);
    S[0][0] = cloud_scale.x;
    S[1][1] = cloud_scale.y;
    S[2][2] = cloud_scale.z;

    mat3 linear = R * S;
    mat3 normal_matrix = transpose(inverse(linear));

    return safe_normalize(normal_matrix * local_n);
}

vec3 transform_point_world(vec3 cloud_rotation,
                           vec3 cloud_scale,
                           vec3 cloud_position,
                           vec3 local_p)
{
    mat3 R = euler_xyz_to_mat3(cloud_rotation);
    vec3 scaled = local_p * cloud_scale;
    return R * scaled + cloud_position;
}

mat3 covariance_from_normal(vec3 raw_n, float eps)
{
    float len2 = dot(raw_n, raw_n);
    if (len2 < 1e-12)
        return mat3(0.0);

    vec3 n = raw_n / sqrt(len2);
    mat3 I = mat3(1.0);
    mat3 nnT = outerProduct(n, n);

    // I - nn^T + eps * nn^T
    return I - (1.0 - eps) * nnT;
}


// uint find_closest_id(vec3 position, float max_dist_sq, float max_corr_dist)
// {
//     const uint pack_bits   = 21u;
//     const uint pack_offset = 1048575u;

//     float voxel_size = 1.0;
//     ivec3 voxel_pos = ivec3(floor(position / voxel_size));

//     // int num_layers = 3;
//     int num_layers = int(ceil(max_corr_dist / voxel_size)) + 1;
    
//     uint min_id = INVALID_ID;
//     float min_dist = max_dist_sq;

//     for (int layer = 0; layer < num_layers; layer++) {
//         const int dim = 1 + layer * 2;

//         for (int y = 0; y < dim; y++) {
//             const bool on_y_edge = (y == 0 || y == dim - 1);

//             for (int z = 0; z < dim; z++) {
//                 const bool on_z_edge = (z == 0 || z == dim - 1);

//                 int x = 0;
//                 while (x < dim) {
//                     ivec3 new_voxel_pos = voxel_pos + ivec3(x, y, z) - ivec3(layer);

//                     uvec2 key = pack_key_uvec2(new_voxel_pos, pack_offset, pack_bits);

//                     uint map_point_id = lookup_chunk(key);
                    
//                     if (map_point_id != INVALID_ID) {
//                         map_point_id = hash_table[map_point_id].hash_value;

//                         vec3 to_point = map_points[map_point_id].position.xyz - position;
//                         float dist_sq = dot(to_point, to_point);

//                         if (dist_sq < min_dist) {
//                             min_dist = dist_sq;
//                             min_id = map_point_id;
//                         }
//                     }

//                     if (on_y_edge || on_z_edge) {
//                         x++;
//                     } else {
//                         x = (x == dim - 1) ? x + 1 : dim - 1;
//                     }
//                 }
//             }
//         }

//         if (min_id != INVALID_ID)
//             break;
//     }
//     return min_id;
// }

uint find_closest_id(
    vec3 position,
    vec4 normal4,
    float max_dist_sq,
    float max_corr_dist,
    float min_normal_dot
)
{
    const uint pack_bits   = 21u;
    const uint pack_offset = 1048575u;

    float voxel_size = 1.0;
    ivec3 voxel_pos = ivec3(floor(position / voxel_size));

    int num_layers = int(ceil(max_corr_dist / voxel_size)) + 1;

    uint min_id = INVALID_ID;
    float min_dist = max_dist_sq;

    vec3 query_normal = normal4.xyz;
    bool use_normal_check = dot(query_normal, query_normal) > 1e-12;

    if (use_normal_check) {
        query_normal = normalize(query_normal);
    }

    for (int layer = 0; layer < num_layers; layer++) {
        const int dim = 1 + layer * 2;

        for (int y = 0; y < dim; y++) {
            const bool on_y_edge = (y == 0 || y == dim - 1);

            for (int z = 0; z < dim; z++) {
                const bool on_z_edge = (z == 0 || z == dim - 1);

                int x = 0;
                while (x < dim) {
                    ivec3 new_voxel_pos = voxel_pos + ivec3(x, y, z) - ivec3(layer);

                    uvec2 key = pack_key_uvec2(new_voxel_pos, pack_offset, pack_bits);

                    uint map_point_id = lookup_chunk(key);

                    if (map_point_id != INVALID_ID) {
                        map_point_id = hash_table[map_point_id].hash_value;

                        vec3 candidate_pos = map_points[map_point_id].position.xyz;
                        vec3 to_point = candidate_pos - position;
                        float dist_sq = dot(to_point, to_point);

                        if (dist_sq < min_dist) {
                            bool normal_ok = true;

                            if (use_normal_check) {
                                vec3 candidate_normal = map_normals[map_point_id].normal.xyz;
                                float nlen_sq = dot(candidate_normal, candidate_normal);

                                if (nlen_sq < 1e-12) {
                                    normal_ok = false;
                                } else {
                                    candidate_normal *= inversesqrt(nlen_sq);

                                    float ndot = dot(query_normal, candidate_normal);

                                    // If normal orientation may be flipped, use this instead:
                                    // ndot = abs(ndot);

                                    if (ndot < min_normal_dot) {
                                        normal_ok = false;
                                    }
                                }
                            }

                            if (normal_ok) {
                                min_dist = dist_sq;
                                min_id = map_point_id;
                            }
                        }
                    }

                    if (on_y_edge || on_z_edge) {
                        x++;
                    } else {
                        x = (x == dim - 1) ? x + 1 : dim - 1;
                    }
                }
            }
        }

        // if (min_id != INVALID_ID)
        //     break;
    }

    return min_id;
}

uint find_closest_id_brute_force(
    vec3 position,
    vec4 normal4,
    float max_dist_sq,
    float max_corr_dist,
    float min_normal_dot
) {
    if (num_map_points == 0u)
        return 0xFFFFFFFFu; // equivalent of -1

    uint min_id = 0xFFFFFFFFu;
    float min_dist = max_dist_sq;

    for (uint i = 0u; i < num_map_points; i++) {
        vec3 target_normal = map_normals[i].normal.xyz;

        // skip invalid normals
        if (dot(target_normal, target_normal) < 1e-12)
            continue;

        vec3 d = map_points[i].position.xyz - position;
        float dist = dot(d, d); // squared distance

        if (dist < min_dist) {
            min_dist = dist;
            min_id = i;
        }
    }

    return min_id;
}

mat3 skew_matrix(vec3 v)
{
    return mat3(
         0.0,  v.z, -v.y,
        -v.z,  0.0,  v.x,
         v.y, -v.x,  0.0
    );
}

void add_mat3_block(inout double H[6][6], int row0, int col0, mat3 M)
{
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            H[row0 + r][col0 + c] += double(M[c][r]); // mat3 is also [col][row]
        }
    }
}

void add_vec3_block(inout double g[6], int row0, vec3 v)
{
    for (int r = 0; r < 3; r++) {
        g[row0 + r] += double(v[r]);
    }
}

bool solve_6x6(in double H_in[6][6], in double g_in[6], out double delta_out[6])
{
    const double EPS = double(1.0e-12);

    // Augmented matrix [H | g], size 6 x 7
    double a[6][7];

    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 6; c++) {
            a[r][c] = H_in[r][c];
        }
        a[r][6] = g_in[r];
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < 6; col++) {
        // Find pivot row
        int pivot_row = col;
        double max_abs = abs(a[col][col]);

        for (int r = col + 1; r < 6; r++) {
            double v = abs(a[r][col]);
            if (v > max_abs) {
                max_abs = v;
                pivot_row = r;
            }
        }

        // Singular / degenerate check
        if (max_abs < EPS) {
            return false;
        }

        // Swap rows if needed
        if (pivot_row != col) {
            for (int c = col; c < 7; c++) {
                double tmp = a[col][c];
                a[col][c] = a[pivot_row][c];
                a[pivot_row][c] = tmp;
            }
        }

        // Eliminate rows below
        for (int r = col + 1; r < 6; r++) {
            double factor = a[r][col] / a[col][col];

            for (int c = col; c < 7; c++) {
                a[r][c] -= factor * a[col][c];
            }
        }
    }

    // Back substitution
    for (int r = 5; r >= 0; r--) {
        double sum = a[r][6];

        for (int c = r + 1; c < 6; c++) {
            sum -= a[r][c] * delta_out[c];
        }

        if (abs(a[r][r]) < EPS) {
            return false;
        }

        delta_out[r] = sum / a[r][r];
    }

    return true;
}

mat3 omega_to_mat3(vec3 omega)
{
    float theta = length(omega);

    if (theta < 1e-12)
        return mat3(1.0);

    vec3 axis = omega / theta;
    mat3 K = skew_matrix(axis);

    // Rodrigues rotation formula
    return mat3(1.0) + sin(theta) * K + (1.0 - cos(theta)) * (K * K);
}

vec3 mat3_to_euler_xyz(mat3 R)
{
    const float EPS = 1e-6;
    const float HALF_PI = 1.57079632679;

    float x, y, z;

    // For R = Rz(z) * Ry(y) * Rx(x):
    // R[0][2] = -sin(y)   because mat[col][row]
    float sy = clamp(-R[0][2], -1.0, 1.0);

    if (sy >= 1.0 - EPS) {
        // y = +pi/2 : gimbal lock
        y = HALF_PI;
        z = 0.0;
        x = atan(R[1][0], R[1][1]);
    }
    else if (sy <= -1.0 + EPS) {
        // y = -pi/2 : gimbal lock
        y = -HALF_PI;
        z = 0.0;
        x = atan(-R[1][0], R[1][1]);
    }
    else {
        y = asin(sy);
        x = atan(R[1][2], R[2][2]);
        z = atan(R[0][1], R[0][0]);
    }

    return vec3(x, y, z);
}

double step()
{
    // const std::vector<PointInstance>& source_points = source_point_cloud.points;
    // const std::vector<PointInstance>& target_points = target_point_cloud.points;

    // if (source_points.size() != source_normals.size()) {
    //     std::cout << "source_points.size() != source_normals.size()\n";
    //     return -1.0;
    // }

    // if (target_points.size() != target_normals.size()) {
    //     std::cout << "target_points.size() != target_normals.size()\n";
    //     return -1.0;
    // }

    float max_corr_dist = 10.0;
    float max_corr_dist_sq = max_corr_dist * max_corr_dist;
    float max_rot = radians(5.0);
    float max_trans = 5.0;
    float gicp_eps = 1e-3;
    // float min_normal_dot = cos(radians(30.0)); 
    float min_normal_dot = -1.0;

    // double H[6][6] = {};
    // double g[6] = {};

    double H[6][6];
    double g[6];

    for (int i = 0; i < 6; ++i) {
        g[i] = 0.0;
        for (int j = 0; j < 6; ++j) {
            H[i][j] = 0.0;
        }
    }

    // Precompute target points, normals, covariances in world space
    // std::vector<glm::vec3> target_points_world;
    // std::vector<glm::vec3> target_normals_world;
    // std::vector<glm::mat3> target_covs_world;

    // target_points_world.reserve(target_points.size());
    // target_normals_world.reserve(target_points.size());
    // target_covs_world.reserve(target_points.size());

    // std::vector<float> source_distances(source_points.size(), -1.0f);
    // std::vector<float> target_indices(source_points.size(), -1.0f);
    float max_new_dist = 0.0f;

    // for (size_t i = 0; i < target_points.size(); i++) {
    //     glm::vec3 q_local = glm::vec3(target_points[i].pos);

    //     // Convert vec4 normal -> vec3 normal (ignore w)
    //     glm::vec3 n_local = glm::vec3(target_normals[i]);
    //     glm::vec3 n_world = transform_normal_world(target_point_cloud, n_local);

    //     target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
    //     target_normals_world.push_back(n_world);
    //     target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));
    // }

    double total_weighted_sq_error = 0.0;
    int valid_count = 0;

    for (uint i = 0; i < num_source_points; i++) {
        
        vec3 p_local = vec3(source_points[i].position);

        // Convert vec4 normal -> vec3 normal (ignore w)
        vec3 n_src_local = vec3(source_normals[i].normal);


        // vec3 transform_point_world(vec3 cloud_rotation,
        //                    vec3 cloud_scale,
        //                    vec3 cloud_position,
        //                    vec3 local_p)

        // Source point and source normal in world space

        vec3 source_rotation = source_location.rotation.xyz;
        vec3 source_scale = vec3(1.0, 1.0, 1.0);
        vec3 source_position = source_location.position.xyz;

        vec3 x = transform_point_world(source_rotation, source_scale, source_position, p_local);

        // vec3 transform_normal_world(vec3 cloud_rotation,
        //                     vec3 cloud_scale,
        //                     vec3 local_n)

        vec3 n_src_world = transform_normal_world(source_rotation, source_scale, n_src_local);

        
        

        if (dot(n_src_world, n_src_world) < 1e-12) {
            continue;
        }

        

        

        // uint find_closest_id(vec3 position, float max_dist_sq)

        // int target_id = find_closest_id_with_valid_normal(
        //     target_points_world,
        //     target_normals_world,
        //     x,
        //     max_corr_dist_sq
        // );

        // uint find_closest_id(vec3 position, float max_dist_sq)

        // uint target_id = find_closest_id(x, vec4(n_src_world, 1.0), max_corr_dist_sq, max_corr_dist, min_normal_dot);
        uint target_id = find_closest_id_brute_force(x, vec4(n_src_world, 1.0), max_corr_dist_sq, max_corr_dist, min_normal_dot);

        

        // source_location.position.x = i;
        
        if (target_id == INVALID_ID) {
            continue;
        }

        vec3 target_position = map_points[target_id].position.xyz;

        // source_distances[i]? = distance(target_position, x);

        // if (source_distances[i] > max_new_dist) {
        //     max_new_dist = source_distances[i];
        // }

        // target_indices[i] = static_cast<float>(target_id);

        // source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);

        vec3 q = target_position;
        vec3 n_tgt_world = map_normals[target_id].normal.xyz;

        // if (dot(n_tgt_world, n_tgt_world) < 1e-12) {
        //     continue;
        // }

        

        // Build source and target local covariances
        // mat3 covariance_from_normal(vec3 raw_n, float eps)

        mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
        
        // target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));

        mat3 C_B = covariance_from_normal(n_tgt_world, gicp_eps);

        // GICP weighting matrix:
        // M = (C_B + C_A)^-1
        mat3 Sigma = C_B + C_A;
        mat3 M = inverse(Sigma);

        vec3 d = q - x;

        // Linearization:
        // x' ≈ x + ω × x + v
        // d' = q - x' ≈ d + x×ω - v
        // so A = [skew(x), -I], residual = d

        // mat3 skew_matrix(vec3 v)
        mat3 B = skew_matrix(x);

        mat3 H00 = transpose(B) * M * B;
        mat3 H01 = -transpose(B) * M;
        mat3 H10 = -M * B;
        mat3 H11 = M;

        vec3 g0 = -transpose(B) * M * d;
        vec3 g1 = M * d;

        // void add_mat3_block(inout double H[6][6], int row0, int col0, mat3 M)

        add_mat3_block(H, 0, 0, H00);
        add_mat3_block(H, 0, 3, H01);
        add_mat3_block(H, 3, 0, H10);
        add_mat3_block(H, 3, 3, H11);

        // void add_vec3_block(inout double g[6], int row0, vec3 v)

        add_vec3_block(g, 0, g0);
        add_vec3_block(g, 3, g1);

        total_weighted_sq_error += dot(d, M * d);
        valid_count++;
    }
    
    if (valid_count < 6) {
        // std::cout << "Too few correspondences\n";
        return -1.0;
    }

    double rmse = sqrt(total_weighted_sq_error / double(valid_count));

    double lambda = 1e-6;
    for (int i = 0; i < 6; i++) {
        H[i][i] += lambda;
    }

    double delta[6];

    for (int i = 0; i < 6; i++)
        delta[i] = 0.0;

    bool ok = solve_6x6(H, g, delta);
    if (!ok) {
        // std::cout << "solve_6x6 failed\n";
        return -1.0;
    }

    vec3 omega = vec3(float(delta[0]), float(delta[1]), float(delta[2]));
    vec3 v = vec3(float(delta[3]), float(delta[4]), float(delta[5]));

    float omega_len = length(omega);
    if (omega_len > max_rot) {
        omega *= max_rot / omega_len;
    }

    float v_len = length(v);
    if (v_len > max_trans) {
        v *= max_trans / v_len;
    }

    // mat3 omega_to_mat3(vec3 omega)

    mat3 dR = omega_to_mat3(omega);

    // mat3 euler_xyz_to_mat3(vec3 euler)

    // source_location.rotation.xyz
    // source_location.position.xyz

    mat3 R_src = euler_xyz_to_mat3(source_location.rotation.xyz);
    mat3 R_src_new = dR * R_src;
    vec3 t_src_new = dR * source_location.position.xyz + v;

    source_location.position.xyz = t_src_new;
    source_location.rotation.xyz = mat3_to_euler_xyz(R_src_new);


    // std::cout << "valid_count = " << valid_count
    //           << ", weighted_rmse = " << rmse
    //           << ", |omega| = " << glm::length(omega)
    //           << ", |v| = " << glm::length(v)
    //           << "\n";

    return rmse;
}



#define STATUS_UNLOCKED 0u
#define STATUS_LOCKED 1u

uniform vec3 uCloudRotation;
uniform vec3 uCloudScale;
uniform vec3 uLocalN;


// vec3 transform_normal_world(vec3 cloud_rotation,
//                             vec3 cloud_scale,
//                             vec3 local_n)


void main() {
    uint source_point_id = gl_GlobalInvocationID.x;
    
    

    // find_closest_id(source_location.position.xyz, vec4(n_src_world, 1.0), max_corr_dist_sq, max_corr_dist, min_normal_dot);



    // location.position.x = 10.0;
    // location.position.y = 12.0;
    // location.position.z = 14.0;

    // location.rotation.x = 1.4;
    // location.rotation.y = 5.4;
    // location.rotation.z = 3.4;
    
    if (source_point_id != 0) return;

    // uEuler

    test_output.output_vector = vec4(transform_normal_world(uCloudRotation, uCloudScale, uLocalN), 999.0);
    // test_output.output_vector = vec4(15, 16, 17, 18);

    // int idx = 0;
    // for (int i = 0; i < 3; i++)
    //     for (int j = 0; j < 3; j++) {
    //         test_output.output_matrix[i][j] = idx;
    //         idx++;
    //     }

    


    // mat3 euler_xyz_to_mat3(vec3 euler)



    // step();

    // source_location.position.x = 10.0;
    // source_location.position.y = 12.0;
    // source_location.position.z = 14.0;

    // source_location.rotation.x = 1.4;
    // source_location.rotation.y = 5.4;
    // source_location.rotation.z = 3.4;

    // step();
    // step();
    // step();
    // step();
    // step();








    
    // struct HAndG {
    //     double h[6][6];
    //     double g[6];
    //     uint status; // lock flag
    // };
    

    // // while (true) {
    // for (int i = 0; i < 100; i++) {
    //     uint old_status = atomicCompSwap(h_and_g.status, STATUS_UNLOCKED, STATUS_LOCKED);

    //     if (old_status == STATUS_UNLOCKED) {
    //         for (int i = 0; i < 6; i++) {
    //             // h_and_g.g[i] += double(1.0);
    //             h_and_g.g[i] = num_source_points;
    //             for (int j = 0; j < 6; j++) {
    //                 // h_and_g.h[i][j] += double(1.0);
    //                 h_and_g.h[i][j] = num_source_points;
    //             }
    //         }

    //         memoryBarrierBuffer();
    //         atomicExchange(h_and_g.status, STATUS_UNLOCKED);
    //         break;
    //     }
    // }

    // }

        
    
    // const uint pack_bits   = 21u;
    // const uint pack_offset = 1048575u;

    // float voxel_size = 1.0f;

    // vec3 source_pos = source_points[source_point_id].position.xyz;
    
    // ivec3 voxel_coord = ivec3(floor(source_pos / voxel_size));

    // uvec2 key = pack_key_uvec2(voxel_coord, pack_offset, pack_bits);



    // uint slot_id = 0u;
    // bool created = false;
    // get_or_create_chunk(key, slot_id, created);

    // uint map_point_id = 0;
    // if (created) {
    //     map_point_id = atomicAdd(num_map_points, 1u);
    //     hash_table[slot_id].hash_value = map_point_id;
    // } else {
    //     map_point_id = hash_table[slot_id].hash_value;
    // }

    // map_points[map_point_id] = source_points[source_point_id];
}
