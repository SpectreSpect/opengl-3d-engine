#include "utils.glsl"

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