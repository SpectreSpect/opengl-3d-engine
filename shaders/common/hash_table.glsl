#include "../utils.glsl"

/*
Для настройки #include можно определить следующие дефайны (не обязательно):
#define TOMB_CHECK_LIST_SIZE n (число n должно быть n = 2^i)
#define NOT_INCLUDE_GET_OR_CREATE (позволяет не подключить get_or_create_chunk())
#define NOT_INCLUDE_LOOKUP_REMOVE (позволяет не подключать lookup_chunk() и remove_from_table())

Также необходимо наличие следующих данных (названия обязательно должны соответсвовать указанным).

Буфферы:
coherent buffer ChunkHashKeys { uvec2 hash_keys[]; };
coherent buffer ChunkHashVals { uint count_tomb; uint  hash_vals[]; };
buffer FreeList { uint free_list[]; }; (только если подключён get_or_create_chunk())
buffer ChunkMetaBuf { ChunkMeta meta[]; }; (только если подключён get_or_create_chunk())


Юниформы:
uniform uint u_hash_table_size;
*/

#ifndef HASH_TABLE_COMMON
#define HASH_TABLE_COMMON

#define SLOT_EMPTY   0xFFFFFFFFu
#define SLOT_LOCKED  0xFFFFFFFEu
#define SLOT_TOMB    0xFFFFFFFDu
#define MAX_PROBES   128u

#ifndef TOMB_CHECK_LIST_SIZE
#define TOMB_CHECK_LIST_SIZE 32u
#endif

const uint TOMB_LIST_MASK = TOMB_CHECK_LIST_SIZE - 1u;

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
#endif

#ifndef NOT_INCLUDE_GET_OR_CREATE
#ifndef HASH_TABLE_GET_OR_CREATE
#define HASH_TABLE_GET_OR_CREATE

uint pop_free_chunk_id() {
    for (;;) {
        uint old_counter = atomicAdd(free_count, 0u);
        if (old_counter == 0u) return INVALID_ID;
        
        if (atomicCompSwap(free_count, old_counter, old_counter - 1u) == old_counter) {
            memoryBarrierBuffer();
            return free_list[old_counter - 1u];
        }
    }
}

// bool get_or_create_chunk(uvec2 key, out uint outId, out bool created) {
//     uint mask = u_hash_table_size - 1u;
//     uint idx  = hash_uvec2(key) & mask;
//     uint last_tomb_id = INVALID_ID;

//     tomb_list_head_id = INVALID_ID;
//     tomb_list_tail_id = INVALID_ID;
//     tomb_list_count_elements = 0u;

//     for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
//         uint v = atomicAdd(hash_vals[idx], 0u);

//         if (v == SLOT_EMPTY || probe == MAX_PROBES) {
//             if (probe >= MAX_PROBES) idx = INVALID_ID;

//             // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
//             if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

//             uint idx_to_create = last_tomb_id == INVALID_ID ? idx : last_tomb_id;
//             uint slot_state = last_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

//             if (idx_to_create == INVALID_ID) {
//                 outId = INVALID_ID;
//                 created = false;
//                 return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди
//             }

//             uint prev = atomicCompSwap(hash_vals[idx_to_create], slot_state, SLOT_LOCKED);
//             if (prev != slot_state) {
//                 // кто-то успел — перепроверяем этот же idx

//                 if (last_tomb_id != INVALID_ID && prev != SLOT_LOCKED) {
//                     // Условие говорит, что мы проверяем tomb_check_list_counter - 1u слот и он оказался занят
//                     last_tomb_id = INVALID_ID;
//                     continue;
//                 }
//                 else {
//                     continue;
//                 }
//             }

//             uint id = pop_free_chunk_id();
//             if (id == INVALID_ID) {
//                 atomicExchange(hash_vals[idx_to_create], slot_state);
//                 outId = INVALID_ID;
//                 created = false;
//                 return false;
//             }

//             if (slot_state == SLOT_TOMB) {
//                 atomicAdd(count_tomb, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как count_tomb > 0)
//             }

//             // meta подготовим ДО публикации id
//             meta[id].used = 1u;
//             meta[id].key_lo = key.x;
//             meta[id].key_hi = key.y;
//             meta[id].dirty_flags = NEED_GENERATION_FLAG_BIT;

//             // публикуем key
//             hash_keys[idx_to_create] = key;

//             // гарантируем, что key/meta видимы до публикации id
//             memoryBarrierBuffer();

//             // публикуем id (и одновременно "анлочим")
//             atomicExchange(hash_vals[idx_to_create], id);

//             outId = id;
//             created = true;
//             return true;
//         }

//         if (v == SLOT_LOCKED) continue;

//         if (v == SLOT_TOMB) {
//             push_tomb_id(idx);
//             idx = (idx + 1u) & mask;
//             probe++;
//             continue;
//         }

//         memoryBarrierBuffer();

//         if (all(equal(hash_keys[idx], key))) {
//             outId = v;
//             created = false;
//             return true;
//         }
        
//         idx = (idx + 1u) & mask;
//         probe++;
//     }

//     outId = INVALID_ID;
//     created = false;
//     return false;
// }

bool get_or_create_chunk(uvec2 key, out uint outId, out bool created) {
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

            if (idx_to_create == INVALID_ID) {
                outId = INVALID_ID;
                created = false;
                return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди
            }

            uint prev = atomicCompSwap(hash_vals[idx_to_create], slot_state, SLOT_LOCKED);
            if (prev != slot_state) {
                // кто-то успел — перепроверяем этот же idx
                if (prev == SLOT_LOCKED) continue;

                if (slot_state == SLOT_TOMB) {
                    // Условие говорит, что мы проверяем tomb_check_list_counter - 1u слот и он оказался занят

                    memoryBarrierBuffer();
                    if (all(equal(hash_keys[idx_to_create], key))) {
                        outId = prev;
                        created = false;
                        return true;
                    }
                    
                    last_tomb_id = INVALID_ID;  
                }

                continue;
            }

            uint id = pop_free_chunk_id();
            if (id == INVALID_ID) {
                atomicExchange(hash_vals[idx_to_create], slot_state);
                outId = INVALID_ID;
                created = false;
                return false;
            }

            if (slot_state == SLOT_TOMB) {
                atomicAdd(count_tomb, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как count_tomb > 0)
            }

            // meta подготовим ДО публикации id
            meta[id].used = 1u;
            meta[id].key_lo = key.x;
            meta[id].key_hi = key.y;
            meta[id].dirty_flags = NEED_GENERATION_FLAG_BIT;

            // публикуем key
            hash_keys[idx_to_create] = key;

            // гарантируем, что key/meta видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            atomicExchange(hash_vals[idx_to_create], id);

            outId = id;
            created = true;
            return true;
        }

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            push_tomb_id(idx);
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        memoryBarrierBuffer();

        if (all(equal(hash_keys[idx], key))) {
            outId = v;
            created = false;
            return true;
        }
        
        idx = (idx + 1u) & mask;
        probe++;
    }

    outId = INVALID_ID;
    created = false;
    return false;
}

// bool get_or_create_chunk(uvec2 key, out uint outId, out bool created) {
//     uint mask = u_hash_table_size - 1u;
//     uint idx  = hash_uvec2(key) & mask;
//     uint first_tomb_id = INVALID_ID;

//     for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
//         uint v = atomicAdd(hash_vals[idx], 0u);

//         if (v == SLOT_LOCKED) continue;

//         if (v == SLOT_TOMB) {
//             if (first_tomb_id == INVALID_ID) first_tomb_id = idx;
//             idx = (idx + 1u) & mask;
//             probe++;
//             continue;
//         }

//         if (v == SLOT_EMPTY) {
//             uint idx_to_create = first_tomb_id == INVALID_ID ? idx : first_tomb_id;
//             uint slot_state = first_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

//             uint prev = atomicCompSwap(hash_vals[idx_to_create], slot_state, SLOT_LOCKED);
//             if (prev != slot_state) {
//                 if (prev == SLOT_LOCKED) continue;
                
//                 if (slot_state == SLOT_TOMB) {
//                     memoryBarrierBuffer();

//                     if (all(equal(hash_keys[idx_to_create], key))) {
//                         outId = prev;
//                         created = false;
//                         return true;
//                     }

//                     first_tomb_id = INVALID_ID;
//                 }
//                 continue;
//             }

//             uint id = pop_free_chunk_id();
//             if (id == INVALID_ID) {
//                 atomicExchange(hash_vals[idx_to_create], slot_state);
//                 outId = INVALID_ID;
//                 created = false;
//                 return false;
//             }

//             if (slot_state == SLOT_TOMB) {
//                 atomicAdd(count_tomb, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как count_tomb > 0)
//             }

//             // meta подготовим ДО публикации id
//             meta[id].used = 1u;
//             meta[id].key_lo = key.x;
//             meta[id].key_hi = key.y;
//             meta[id].dirty_flags = NEED_GENERATION_FLAG_BIT;

//             // публикуем key
//             hash_keys[idx_to_create] = key;

//             // гарантируем, что key/meta видимы до публикации id
//             memoryBarrierBuffer();

//             // публикуем id (и одновременно "анлочим")
//             atomicExchange(hash_vals[idx_to_create], id);

//             outId = id;
//             created = true;
//             return true;
//         }

//         memoryBarrierBuffer();

//         if (all(equal(hash_keys[idx], key))) {
//             outId = v;
//             created = false;
//             return true;
//         }
        
//         idx = (idx + 1u) & mask;
//         probe++;
//     }

//     outId = INVALID_ID;
//     created = false;
//     return false;
// }
#endif
#endif

#ifndef NOT_INCLUDE_LOOKUP_REMOVE
#ifndef HASH_TABLE_LOOKUP_REMOVE_CHUNK
#define HASH_TABLE_LOOKUP_REMOVE_CHUNK
uint lookup_chunk(uvec2 key, bool read_only = false) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint v = read_only ? hash_vals[idx] : atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_TOMB) {
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        if (v == SLOT_EMPTY) return INVALID_ID;

        // Если мы сюда дошли, значит в слоте стоит чья-то запись. Нужно прочитать ключ
        // Но чтобы прочитать ключ необходимо тоже залочить! (иначе во время прочтения его состояние может уже измениться) 
        if (!read_only)
            memoryBarrierBuffer();

        if (all(equal(hash_keys[idx], key))) 
            return v;
        
        idx = (idx + 1u) & mask;
        probe++;
    }

    return INVALID_ID;
}

bool remove_from_table(uvec2 key) {
    uint mask = u_hash_table_size - 1u;
    uint idx  = hash_uvec2(key) & mask;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint v = atomicAdd(hash_vals[idx], 0u);

        if (v == SLOT_LOCKED) continue;

        if (v == SLOT_EMPTY) return false;

        if (v == SLOT_TOMB) { 
            idx = (idx + 1u) & mask;
            probe++;
            continue;
        }

        memoryBarrierBuffer();

        if (all(equal(hash_keys[idx], key))) {
            atomicExchange(hash_vals[idx], SLOT_TOMB); // Удаляем слот
            atomicAdd(count_tomb, 1u);
            return true;
        }

        idx = (idx + 1u) & mask;
        probe++;
    }
    return false;
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
                if (prev == SLOT_LOCKED) continue;

                if (slot_state == SLOT_TOMB) {
                    // Условие говорит, что мы проверяем tomb_check_list_counter - 1u слот и он оказался занят

                    memoryBarrierBuffer();
                    if (all(equal(hash_keys[idx_to_create], key))) {
                        return false;
                    }
                    
                    last_tomb_id = INVALID_ID;  
                }

                continue;
            }

            if (slot_state == SLOT_TOMB) {
                atomicAdd(count_tomb, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как count_tomb > 0)
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

        // if (atomicCompSwap(hash_vals[idx], v, SLOT_LOCKED) == v) {
        //     // Залочили слот - можем читать ключ
        //     if (all(equal(hash_keys[idx], key))) {
        //         atomicExchange(hash_vals[idx], v); // Убираем блокировку
        //         return false;
        //     }

        //     atomicExchange(hash_vals[idx], v); // Убираем блокировку
        // } else {
        //     continue; // Не получилось захватить. Попробуем ещё раз.
        // }

        memoryBarrierBuffer();

        if (all(equal(hash_keys[idx], key))) {
            return false;
        }

        idx = (idx + 1u) & mask;
        probe++;
    }

    return false;
}
#endif
#endif