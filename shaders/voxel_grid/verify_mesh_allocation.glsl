#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu


struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=0) readonly buffer LocalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_local[]; }; 
layout(std430, binding=1) buffer GlobalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 

struct CountFreePages {uint count_vb_free_pages; uint count_ib_free_pages; };
layout(std430, binding=2) readonly buffer CountFreePagesBuf { CountFreePages pages_counters; };

layout(std430, binding=3) readonly buffer DirtyListBuf { uint dirty_list[]; };

struct FrameCounters {uint write_count; uint dirty_count; uint cmd_count; uint free_count; uint failed_dirty_count; };
layout(std430, binding=4) buffer FrameCountersBuf { FrameCounters counters; };

// ---- NEW: VB pool ----
layout(std430, binding=5) coherent buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=6) coherent buffer VBNext  { uint vb_next[];  };
layout(std430, binding=7) coherent buffer VBState { uint vb_state[]; };

// ---- NEW: IB pool ----
layout(std430, binding=8) coherent buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=9) coherent buffer IBNext  { uint ib_next[];  };
layout(std430, binding=10) coherent buffer IBState { uint ib_state[]; };

layout(std430, binding=11) buffer VbAllocStackBuf { uint vb_alloc_stack_counter; uvec3 vb_alloc_stack[]; };
layout(std430, binding=12) buffer IbAllocStackBuf { uint ib_alloc_stack_counter; uvec3 ib_alloc_stack[]; };
layout(std430, binding=13) buffer DebugBuffer { uint stats[]; };

uniform uint u_vb_max_order;
uniform uint u_ib_max_order;
uniform uint u_min_free_pages;

// ---- state packing ----
const uint ST_MASK_BITS = 4;
const uint ST_FREE    = 0u;
const uint ST_ALLOC   = 1u;
const uint ST_MERGED  = 2u;
const uint ST_MERGING = 3u;
const uint ST_READY = 4u;
const uint ST_CONCEDED = 5u;
const uint ST_MASK   = (1u << ST_MASK_BITS) - 1u;

const uint HEAD_LOCK = 0xFFFFFFFEu;

const uint OP_ALLOC = 0u;
const uint OP_FREE = 1u;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }

// При вставке состояние страницы не должно быть ST_FREE!!! Функция сама пометит страницу свободной.
bool vb_push_free(uint order, uint startPage) {
    // Изначально состояние вставляемой страницы (startPage) либо ST_ALLOC, либо ST_MERGED - в противном
    // случае вставка не удастся. Состояние ST_ALLOC будет только при освобождении уже занятой 
    // страницы (со слиянием). Состояние ST_MERGED будет только при освобождении страницы при делении
    // большей, чтобы часть из неё выделить (в теории это значит, что страница должна быть именно нечётной,
    // то есть правой, если я ничего не путаю). Эту информацию можно использовать для отладки!!!


    // Сначала необходимо сделать информацию о странице полностью корректной. Начнём с состояния.

    // Пытаемся убедиться, что пушим страницу только мы - это определяет владение страницей для операции push.
    for (;;) {
        uint prev_state = atomicAdd(vb_state[startPage], 0u);
        uint prev_state_kind = prev_state & ST_MASK;
        if (prev_state_kind == ST_FREE)
            return false; // Значит страница либо уже в списке, либо кто-то первее нас решил вставить её.

        if (atomicCompSwap(vb_state[startPage], prev_state, pack_state(order, ST_FREE)) == prev_state)
            break; // Здесь страницей владеем только мы. Теперь можем вставлять в список.
        
        // Кто-то успел поменять состояние страницы - возможно её вставляют в список вместо нас.
        // Проверим это в новой итерации цикла.
    }

    // Здесь образуется окно, когда мы ещё не опубликовали страницу, но её состояние уже ST_FREE.
    // По этой причине в этот промужеток времени её могут слить (сделать состояние ST_MERGED), но
    // это нормально - тогда наша страница просто станет протухшей (stale node), а такие спокойно
    // умеют жить в списке.

    for (;;) {
        uint old_h = atomicAdd(vb_heads[order], 0u);

        if (old_h == HEAD_LOCK) 
            continue; // Списком владеет кто-то другой - ждём
        
        // Вершина может быть равна INVALID_ID, если список пустой. Это тоже нормально - в таком
        // случае следующем элементом списка будет INVALID_ID.

        // ВОЗМОЖНО ИМЕЕТ СМЫСЛ ТОЖЕ ИСПОЛЬЗОВАТЬ ЗДЕСЬ HEAD_LOCK, НО ПОКА ПОПРОБУЕМ БЕЗ НЕГО
        // if (atomicCompSwap(vb_heads[order], old_h, HEAD_LOCK) != old_h) // Забрали владение списком себе
        //     continue; // Кто-то успел забрать первее - ждём

        // Здесь владеем страницей только мы, поэтому можем спокойно записывать значение без атомиков.
        atomicExchange(vb_next[startPage], old_h);
        
        memoryBarrierBuffer(); // Чтобы сначала было опубликованно vb_next[startPage], а только потом vb_heads[order]
        
        // Если никто не успел за это время поменять вершину, то можем заменять её на себя - информация будет сразу актуальной.
        if (atomicCompSwap(vb_heads[order], old_h, startPage) == old_h) {
            // atomicAdd(pages_counters.count_vb_free_pages, 1u << order);
            return true; // Удалось вставить себя на вершину списка. Можем выходить из функции.
        }
        
        // Кто-то первее нас изменил вершину списка. Повторяем попытку вставки снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return false;
}

uint vb_pop_free(uint order) {
    for (;;) {
        uint old_h = atomicAdd(vb_heads[order], 0u);

        if (old_h == INVALID_ID)
            return INVALID_ID; // Список пустой - выходим

        if (old_h == HEAD_LOCK) 
            continue; // Списком владеет кто-то другой - ждём

        if (atomicCompSwap(vb_heads[order], old_h, HEAD_LOCK) != old_h) // Забрали владение списком себе
            continue; // Кто-то успел забрать первее - ждём
        
        memoryBarrierBuffer(); // Чтобы vb_next[old_h] успело "доехать" и соответствовало текущему old_h, а не было старым

        uint next = atomicAdd(vb_next[old_h], 0u); // В теории можно без атомика, тк владеем нодой только мы
        atomicExchange(vb_heads[order], next); // Сразу вставляем, чтобы минимизировать время блокировки списка

        // Здесь нодой old_h владеем полностью только мы - теперь нужно либо выкинуть её (если stale), либо взять
        
        uint alloc_state = pack_state(order, ST_ALLOC);
        uint free_state = pack_state(order, ST_FREE);
        // Проверяем, является ли страница свободной (ST_FREE) или это протухшей stale node (ST_MERGED).
        // Если страница свободна, то помечаем её как "выделенная" (ST_ALLOC).
        if (atomicCompSwap(vb_state[old_h], free_state, alloc_state) == free_state) {
            // Страница оказалась свободной и мы заменили её на состояние ST_ALLOC. Теперь можем возвращать old_h выходить из функции.
            // atomicAdd(pages_counters.count_vb_free_pages, 0u - (1u << order));
            return old_h; 
        }

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return INVALID_ID;
}

//Освобождать можно только уже выделенную память (та, что имеет состояние ST_ALLOC)!!! Иначе работать не будет!
bool vb_free_pages(uint start, uint order) {
    // Освобождаем нашу страницу, и, если получится, пытаемся
    // слить её с соседями.

    // Сначала нужно убедиться в том, что никто кроме нас не хочет освобождать ту же страницу.
    uint expected_state = pack_state(order, ST_ALLOC);
    if (atomicCompSwap(vb_state[start], expected_state, pack_state(order, ST_MERGING)) != expected_state) {
        // Значит страницу либо кто-то уже освобождает (ST_MERGING), либо функция была вызвана для страницы 
        // с неправильным состоянием (order != page_order, page_state == ST_MERGING || ST_FREE || ST_MERGED)
        return false;
    }

    // Здесь страницей владеем только мы (ST_MERGING), поэтому спокойно освобождаем память.

    // Сливаем страницы - для этого просто помечаем их как ST_MERGED если они свободны (ST_FREE).
    // Так, если эти страницы ранее были в связанном списке, они станут протухшими (stale), но
    // это нормально.
    

    uint cur_start = start;
    uint cur_order = order;
    bool we_are_ready = false;
    uint attempt = 0;
    while (cur_order < u_vb_max_order) {
        memoryBarrierBuffer(); // На всякий случай.

        uint buddy_size = 1u << cur_order;
        uint start_buddy = cur_start ^ buddy_size; // 1010101010001111000000
        if (start_buddy + buddy_size >= (1u << u_vb_max_order)) break;

        // uint start_buddy = (cur_start / (1u << cur_order)) % 2;
        
        uint buddy_state = atomicAdd(vb_state[start_buddy], 0u);
        uint buddy_kind = buddy_state & ST_MASK;
        uint buddy_order = buddy_state >> ST_MASK_BITS;

        if (buddy_order != cur_order) {
            // В этом случае, даже если buddy свободен (ST_FREE), мы не можем его слить,
            // так как между нами и ним будет промежуток памяти, почти навярняка ST_ALLOC.
            // Если же же buddy ST_MERGING, то ситуация посложнее. Это значит, что buddy 
            // сейчас сливает кто-то другой. Тут может быть два случая - либо buddy всё-таки
            // сможет "дорасти" до нашего order, либо он не сможет "дорасти", так как между
            // нами и ним будет ST_ALLOC. В случае, если он не сможет дорасти, в целом, понятно,
            // что сливать его не имеет смысла - мы так и так не смогли бы это сделать. Но если
            // он всё-таки сможет дорасти, тогда, в теории, мы могли бы его слить. Но мы заранее
            // не знаем какой из двух случаев выпадет, поэтому в обоих просто выходим (break) -
            // так, если buddy сможет дорасти, он сам нас сольёт, если нет - мы бы его и не слили и
            // нам так и так пришлось бы уйти.

            // Поэтому здесь единственно верным решением будет прекратить слияние.
            break;
        } else if (buddy_kind == ST_READY && buddy_order == cur_order) {
            // Можем уходить - buddy закончит слияние за нас.
            
            return true;
        } else if ((buddy_kind == ST_MERGING) && buddy_order == cur_order) {
            // Значит кто-то уже сливает buddy. В этом случае, если 
            if (((cur_start >> cur_order) & 1u) == 1u) { // 000000101000
                // Мы справа, поэтому уступаем - слияние продолжит buddy вместо нас.
                
                // Но не уходим. Нужно убедиться, что buddy готов забирать память, а не решил сам уйти (если он
                // не успел увидеть, что мы тоже ST_MERGING, он может решить закончить слияние).
                // continue;
                atomicExchange(vb_state[cur_start], pack_state(cur_order, ST_CONCEDED));
                return true;
                // return vb_push_free(order, start);
            } else {
                // Мы слева - нам необходимо продолжить слияние. Это можно сделать только после того, как buddy
                // покажет, что он уступил (поставит состояние ST_CONCEDED). Поэтому пока ждём.

                // Помечаем себя как ST_READY, чтобы buddy мог спокойно отдать память и уйти.
                // atomicExchange(vb_state[cur_start], pack_state(cur_order, ST_READY));
                // we_are_ready = true;
                attempt++;
                if (attempt > 64u) break;

                continue;
                // return vb_push_free(order, start);
            }
        } else if ((buddy_kind == ST_FREE || buddy_kind == ST_CONCEDED) && buddy_order == cur_order) {
            if (buddy_kind == ST_CONCEDED) {
                atomicAdd(stats[30], 1u);
            }

            if (we_are_ready) {
                // Buddy успешно уступил память - забираем её себе.
                atomicExchange(vb_state[cur_start], pack_state(cur_order, ST_MERGING));
                we_are_ready = false;
            }
            
            uint new_cur_start = min(cur_start, start_buddy);
            uint new_buddy_state = new_cur_start == start_buddy ? pack_state(cur_order + 1, ST_MERGING) : pack_state(cur_order, ST_MERGED);

            // Здесь buddy является целым свободным куском памяти размером cur_order - поэтому внутри
            // него никаких действий происходить не будет (слияний, выделений). А если и будет - мы
            // это сразу узнаем, так как buddy_state изменится. Поэтому если он не изменился, работаем над ним.
            uint comp_buddy_state = atomicCompSwap(vb_state[start_buddy], buddy_state, new_buddy_state); // В новом состоянии buddy трогать точно никто не будет.
            uint comp_buddy_kind = comp_buddy_state & ST_MASK; 
            if (comp_buddy_kind == ST_MERGING) {
                continue; // Значит кто-то начал сливать. Повторяем итерацию для корректной обработки случая.
                // return vb_push_free(order, start);
            }

            if (comp_buddy_state != buddy_state) {
                // Значит buddy кто-то успел забрать и он уже не ST_FREE. Дальше мы уже не можем сливать, поэтому выходим.
                break;
            }

            uint new_cur_state = new_cur_start == cur_start ? pack_state(cur_order + 1, ST_MERGING) : pack_state(cur_order, ST_MERGED);
            atomicExchange(vb_state[cur_start], new_cur_state);

            cur_start = new_cur_start;
            cur_order++;
        } else {
            // buddy не подходит для merge
            break;
        }

        // НЕ ЗАБЫТЬ УЧЕСТЬ, КОГДА МЫ МЕНЬШЕ BUDDY!!! + 
        // НЕ ЗАБЫТЬ ОБРАБОТАТЬ СОСТОЯНИЕ ST_MERGING_BUDDY!!! + 
        // НЕ ЗАБЫТЬ ПОМЕЧАТЬ ВСЕГДА СЕБЯ КАК ST_MERGING!!! + 
        // НЕ ЗАБЫТЬ ОБРАБОТАТЬ СИТУАЦИЮ, ЕСЛИ НАС СЛИЛ BUDDY, НО МЫ НЕ УСПЕЛИ УВИДЕТЬ, ЧТО ОН ST_MERGING!!! + 
    }

    return vb_push_free(cur_order, cur_start);
    // return vb_push_free(order, start);
}

// При вставке состояние страницы не должно быть ST_FREE!!! Функция сама пометит страницу свободной.
bool ib_push_free(uint order, uint startPage) {
    // Изначально состояние вставляемой страницы (startPage) либо ST_ALLOC, либо ST_MERGED - в противном
    // случае вставка не удастся. Состояние ST_ALLOC будет только при освобождении уже занятой 
    // страницы (со слиянием). Состояние ST_MERGED будет только при освобождении страницы при делении
    // большей, чтобы часть из неё выделить (в теории это значит, что страница должна быть именно нечётной,
    // то есть правой, если я ничего не путаю). Эту информацию можно использовать для отладки!!!


    // Сначала необходимо сделать информацию о странице полностью корректной. Начнём с состояния.

    // Пытаемся убедиться, что пушим страницу только мы - это определяет владение страницей для операции push.
    for (;;) {
        uint prev_state = atomicAdd(ib_state[startPage], 0u);
        uint prev_state_kind = prev_state & ST_MASK;
        if (prev_state_kind == ST_FREE)
            return false; // Значит страница либо уже в списке, либо кто-то первее нас решил вставить её.

        if (atomicCompSwap(ib_state[startPage], prev_state, pack_state(order, ST_FREE)) == prev_state)
            break; // Здесь страницей владеем только мы. Теперь можем вставлять в список.
        
        // Кто-то успел поменять состояние страницы - возможно её вставляют в список вместо нас.
        // Проверим это в новой итерации цикла.
    }

    // Здесь образуется окно, когда мы ещё не опубликовали страницу, но её состояние уже ST_FREE.
    // По этой причине в этот промужеток времени её могут слить (сделать состояние ST_MERGED), но
    // это нормально - тогда наша страница просто станет протухшей (stale node), а такие спокойно
    // умеют жить в списке.

    for (;;) {
        uint old_h = atomicAdd(ib_heads[order], 0u);

        if (old_h == HEAD_LOCK) 
            continue; // Списком владеет кто-то другой - ждём
        
        // Вершина может быть равна INVALID_ID, если список пустой. Это тоже нормально - в таком
        // случае следующем элементом списка будет INVALID_ID.

        // ВОЗМОЖНО ИМЕЕТ СМЫСЛ ТОЖЕ ИСПОЛЬЗОВАТЬ ЗДЕСЬ HEAD_LOCK, НО ПОКА ПОПРОБУЕМ БЕЗ НЕГО
        // if (atomicCompSwap(ib_heads[order], old_h, HEAD_LOCK) != old_h) // Забрали владение списком себе
        //     continue; // Кто-то успел забрать первее - ждём

        // Здесь владеем страницей только мы, поэтому можем спокойно записывать значение без атомиков.
        atomicExchange(ib_next[startPage], old_h);
        
        memoryBarrierBuffer(); // Чтобы сначала было опубликованно ib_next[startPage], а только потом ib_heads[order]
        
        // Если никто не успел за это время поменять вершину, то можем заменять её на себя - информация будет сразу актуальной.
        if (atomicCompSwap(ib_heads[order], old_h, startPage) == old_h) {
            // atomicAdd(pages_counters.count_ib_free_pages, 1u << order);
            return true; // Удалось вставить себя на вершину списка. Можем выходить из функции.
        }
        
        // Кто-то первее нас изменил вершину списка. Повторяем попытку вставки снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return false;
}

uint ib_pop_free(uint order) {
    for (;;) {
        uint old_h = atomicAdd(ib_heads[order], 0u);

        if (old_h == INVALID_ID)
            return INVALID_ID; // Список пустой - выходим

        if (old_h == HEAD_LOCK) 
            continue; // Списком владеет кто-то другой - ждём

        if (atomicCompSwap(ib_heads[order], old_h, HEAD_LOCK) != old_h) // Забрали владение списком себе
            continue; // Кто-то успел забрать первее - ждём
        
        memoryBarrierBuffer(); // Чтобы ib_next[old_h] успело "доехать" и соответствовало текущему old_h, а не было старым

        uint next = atomicAdd(ib_next[old_h], 0u); // В теории можно без атомика, тк владеем нодой только мы
        atomicExchange(ib_heads[order], next); // Сразу вставляем, чтобы минимизировать время блокировки списка

        // Здесь нодой old_h владеем полностью только мы - теперь нужно либо выкинуть её (если stale), либо взять
        
        uint alloc_state = pack_state(order, ST_ALLOC);
        uint free_state = pack_state(order, ST_FREE);
        // Проверяем, является ли страница свободной (ST_FREE) или это протухшей stale node (ST_MERGED).
        // Если страница свободна, то помечаем её как "выделенная" (ST_ALLOC).
        if (atomicCompSwap(ib_state[old_h], free_state, alloc_state) == free_state) {
            // Страница оказалась свободной и мы заменили её на состояние ST_ALLOC. Теперь можем возвращать old_h выходить из функции.
            // atomicAdd(pages_counters.count_ib_free_pages, 0u - (1u << order));
            return old_h; 
        }

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }
    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return INVALID_ID;
}

// //Освобождать можно только уже выделенную память (та, что имеет состояние ST_ALLOC)!!! Иначе работать не будет!
// bool ib_free_pages(uint start, uint order) {
//     // Освобождаем нашу страницу, и, если получится, пытаемся
//     // слить её с соседями.

//     // Сначала нужно убедиться в том, что никто кроме нас не хочет освобождать ту же страницу.
//     uint expected_state = pack_state(order, ST_ALLOC);
//     if (atomicCompSwap(ib_state[start], expected_state, pack_state(order, ST_MERGING)) != expected_state) {
//         // Значит страницу либо кто-то уже освобождает (ST_MERGING), либо функция была вызвана для страницы 
//         // с неправильным состоянием (order != page_order, page_state == ST_MERGING || ST_FREE || ST_MERGED)
//         return false;
//     }

//     // Здесь страницей владеем только мы (ST_MERGING), поэтому спокойно освобождаем память.

//     // Сливаем страницы - для этого просто помечаем их как ST_MERGED если они свободны (ST_FREE).
//     // Так, если эти страницы ранее были в связанном списке, они станут протухшими (stale), но
//     // это нормально.
    

//     uint cur_start = start;
//     uint cur_order = order;
//     bool we_are_ready = false;
//     while (cur_order < u_ib_max_order) {
//         memoryBarrierBuffer(); // На всякий случай.

//         uint start_buddy = cur_start ^ (1u << cur_order);
        
//         uint buddy_state = atomicAdd(ib_state[start_buddy], 0u);
//         uint buddy_kind = buddy_state & ST_MASK;
//         uint buddy_order = buddy_state >> ST_MASK_BITS;

//         if (buddy_order != cur_order) {
//             // В этом случае, даже если buddy свободен (ST_FREE), мы не можем его слить,
//             // так как между нами и ним будет промежуток памяти, почти навярняка ST_ALLOC.
//             // Если же же buddy ST_MERGING, то ситуация посложнее. Это значит, что buddy 
//             // сейчас сливает кто-то другой. Тут может быть два случая - либо buddy всё-таки
//             // сможет "дорасти" до нашего order, либо он не сможет "дорасти", так как между
//             // нами и ним будет ST_ALLOC. В случае, если он не сможет дорасти, в целом, понятно,
//             // что сливать его не имеет смысла - мы так и так не смогли бы это сделать. Но если
//             // он всё-таки сможет дорасти, тогда, в теории, мы могли бы его слить. Но мы заранее
//             // не знаем какой из двух случаев выпадет, поэтому в обоих просто выходим (break) -
//             // так, если buddy сможет дорасти, он сам нас сольёт, если нет - мы бы его и не слили и
//             // нам так и так пришлось бы уйти.

//             // Поэтому здесь единственно верным решением будет прекратить слияние.
//             break;
//         } else if (buddy_kind == ST_READY && buddy_order == cur_order) {
//             // Можем уходить - buddy закончит слияние за нас.
//             atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_CONCEDED));
//             return true;
//         } else if ((buddy_kind == ST_MERGING) && buddy_order == cur_order) {
//             // Значит кто-то уже сливает buddy. В этом случае, если 
//             if (((cur_start >> cur_order) & 1u) == 1u) {
//                 // Мы справа, поэтому уступаем - слияние продолжит buddy вместо нас.
                
//                 // Но не уходим. Нужно убедиться, что buddy готов забирать память, а не решил сам уйти (если он
//                 // не успел увидеть, что мы тоже ST_MERGING, он может решить закончить слияние).
//                 continue;
//             } else {
//                 // Мы слева - нам необходимо продолжить слияние. Это можно сделать только после того, как buddy
//                 // покажет, что он уступил (поставит состояние ST_CONCEDED). Поэтому пока ждём.

//                 // Помечаем себя как ST_READY, чтобы buddy мог спокойно отдать память и уйти.
//                 atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_READY));
//                 we_are_ready = true;
//                 continue;
//             }
//         } else if ((buddy_kind == ST_FREE || buddy_kind == ST_CONCEDED) && buddy_order == cur_order) {
//             if (we_are_ready) {
//                 // Buddy успешно уступил память - забираем её себе.
//                 atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_MERGING));
//                 we_are_ready = false;
//             }
            
//             uint new_cur_start = min(cur_start, start_buddy);
//             uint new_buddy_state = new_cur_start == start_buddy ? pack_state(cur_order + 1, ST_MERGING) : pack_state(cur_order, ST_MERGED);

//             // Здесь buddy является целым свободным куском памяти размером cur_order - поэтому внутри
//             // него никаких действий происходить не будет (слияний, выделений). А если и будет - мы
//             // это сразу узнаем, так как buddy_state изменится. Поэтому если он не изменился, работаем над ним.
//             uint comp_buddy_state = atomicCompSwap(ib_state[start_buddy], buddy_state, new_buddy_state); // В новом состоянии buddy трогать точно никто не будет.
//             uint comp_buddy_kind = comp_buddy_state & ST_MASK; 
//             if (comp_buddy_kind == ST_MERGING) {
//                 continue; // Значит кто-то начал сливать. Повторяем итерацию для корректной обработки случая.
//             }

//             if (comp_buddy_state != buddy_state) {
//                 // Значит buddy кто-то успел забрать и он уже не ST_FREE. Дальше мы уже не можем сливать, поэтому выходим.
//                 break;
//             }

//             uint new_cur_state = new_cur_start == cur_start ? pack_state(cur_order + 1, ST_MERGING) : pack_state(cur_order, ST_MERGED);
//             atomicExchange(ib_state[cur_start], new_cur_state);

//             cur_start = new_cur_start;
//             cur_order++;
//         } else {
//             // buddy не подходит для merge
//             break;
//         }

//         // НЕ ЗАБЫТЬ УЧЕСТЬ, КОГДА МЫ МЕНЬШЕ BUDDY!!! + 
//         // НЕ ЗАБЫТЬ ОБРАБОТАТЬ СОСТОЯНИЕ ST_MERGING_BUDDY!!! + 
//         // НЕ ЗАБЫТЬ ПОМЕЧАТЬ ВСЕГДА СЕБЯ КАК ST_MERGING!!! + 
//         // НЕ ЗАБЫТЬ ОБРАБОТАТЬ СИТУАЦИЮ, ЕСЛИ НАС СЛИЛ BUDDY, НО МЫ НЕ УСПЕЛИ УВИДЕТЬ, ЧТО ОН ST_MERGING!!! + 
//     }

//     return ib_push_free(cur_order, cur_start);
//     // return ib_push_free(order, start);
// }

//Освобождать можно только уже выделенную память (та, что имеет состояние ST_ALLOC)!!! Иначе работать не будет!
bool ib_free_pages(uint start, uint order) {
    // Освобождаем нашу страницу, и, если получится, пытаемся
    // слить её с соседями.

    // Сначала нужно убедиться в том, что никто кроме нас не хочет освобождать ту же страницу.
    uint expected_state = pack_state(order, ST_ALLOC);
    if (atomicCompSwap(ib_state[start], expected_state, pack_state(order, ST_MERGING)) != expected_state) {
        // Значит страницу либо кто-то уже освобождает (ST_MERGING), либо функция была вызвана для страницы 
        // с неправильным состоянием (order != page_order, page_state == ST_MERGING || ST_FREE || ST_MERGED)
        return false;
    }

    // Здесь страницей владеем только мы (ST_MERGING), поэтому спокойно освобождаем память.

    // Сливаем страницы - для этого просто помечаем их как ST_MERGED если они свободны (ST_FREE).
    // Так, если эти страницы ранее были в связанном списке, они станут протухшими (stale), но
    // это нормально.
    

    uint cur_start = start;
    uint cur_order = order;
    bool we_are_ready = false;
    uint attempt = 0;
    while (cur_order < u_ib_max_order) {
        memoryBarrierBuffer(); // На всякий случай.

        uint buddy_size = 1u << cur_order;
        uint start_buddy = cur_start ^ buddy_size; // 1010101010001111000000
        if (start_buddy + buddy_size >= (1u << u_ib_max_order)) break;

        // uint start_buddy = (cur_start / (1u << cur_order)) % 2;
        
        uint buddy_state = atomicAdd(ib_state[start_buddy], 0u);
        uint buddy_kind = buddy_state & ST_MASK;
        uint buddy_order = buddy_state >> ST_MASK_BITS;

        if (buddy_order != cur_order) {
            // В этом случае, даже если buddy свободен (ST_FREE), мы не можем его слить,
            // так как между нами и ним будет промежуток памяти, почти навярняка ST_ALLOC.
            // Если же же buddy ST_MERGING, то ситуация посложнее. Это значит, что buddy 
            // сейчас сливает кто-то другой. Тут может быть два случая - либо buddy всё-таки
            // сможет "дорасти" до нашего order, либо он не сможет "дорасти", так как между
            // нами и ним будет ST_ALLOC. В случае, если он не сможет дорасти, в целом, понятно,
            // что сливать его не имеет смысла - мы так и так не смогли бы это сделать. Но если
            // он всё-таки сможет дорасти, тогда, в теории, мы могли бы его слить. Но мы заранее
            // не знаем какой из двух случаев выпадет, поэтому в обоих просто выходим (break) -
            // так, если buddy сможет дорасти, он сам нас сольёт, если нет - мы бы его и не слили и
            // нам так и так пришлось бы уйти.

            // Поэтому здесь единственно верным решением будет прекратить слияние.
            break;
        } else if (buddy_kind == ST_READY && buddy_order == cur_order) {
            // Можем уходить - buddy закончит слияние за нас.
            
            return true;
        } else if ((buddy_kind == ST_MERGING) && buddy_order == cur_order) {
            // Значит кто-то уже сливает buddy. В этом случае, если 
            if (((cur_start >> cur_order) & 1u) == 1u) { // 000000101000
                // Мы справа, поэтому уступаем - слияние продолжит buddy вместо нас.
                
                // Но не уходим. Нужно убедиться, что buddy готов забирать память, а не решил сам уйти (если он
                // не успел увидеть, что мы тоже ST_MERGING, он может решить закончить слияние).
                // continue;
                atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_CONCEDED));
                return true;
                // return ib_push_free(order, start);
            } else {
                // Мы слева - нам необходимо продолжить слияние. Это можно сделать только после того, как buddy
                // покажет, что он уступил (поставит состояние ST_CONCEDED). Поэтому пока ждём.

                // Помечаем себя как ST_READY, чтобы buddy мог спокойно отдать память и уйти.
                // atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_READY));
                // we_are_ready = true;
                attempt++;
                if (attempt > 64u) break;

                continue;
                // return ib_push_free(order, start);
            }
        } else if ((buddy_kind == ST_FREE || buddy_kind == ST_CONCEDED) && buddy_order == cur_order) {
            if (buddy_kind == ST_CONCEDED) {
                atomicAdd(stats[30], 1u);
            }

            if (we_are_ready) {
                // Buddy успешно уступил память - забираем её себе.
                atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_MERGING));
                we_are_ready = false;
            }
            
            uint new_cur_start = min(cur_start, start_buddy);
            uint new_buddy_state = new_cur_start == start_buddy ? pack_state(cur_order + 1, ST_MERGING) : pack_state(cur_order, ST_MERGED);

            // Здесь buddy является целым свободным куском памяти размером cur_order - поэтому внутри
            // него никаких действий происходить не будет (слияний, выделений). А если и будет - мы
            // это сразу узнаем, так как buddy_state изменится. Поэтому если он не изменился, работаем над ним.
            uint comp_buddy_state = atomicCompSwap(ib_state[start_buddy], buddy_state, new_buddy_state); // В новом состоянии buddy трогать точно никто не будет.
            uint comp_buddy_kind = comp_buddy_state & ST_MASK; 
            if (comp_buddy_kind == ST_MERGING) {
                continue; // Значит кто-то начал сливать. Повторяем итерацию для корректной обработки случая.
                // return ib_push_free(order, start);
            }

            if (comp_buddy_state != buddy_state) {
                // Значит buddy кто-то успел забрать и он уже не ST_FREE. Дальше мы уже не можем сливать, поэтому выходим.
                break;
            }

            uint new_cur_state = new_cur_start == cur_start ? pack_state(cur_order + 1, ST_MERGING) : pack_state(cur_order, ST_MERGED);
            atomicExchange(ib_state[cur_start], new_cur_state);

            cur_start = new_cur_start;
            cur_order++;
        } else {
            // buddy не подходит для merge
            break;
        }

        // НЕ ЗАБЫТЬ УЧЕСТЬ, КОГДА МЫ МЕНЬШЕ BUDDY!!! + 
        // НЕ ЗАБЫТЬ ОБРАБОТАТЬ СОСТОЯНИЕ ST_MERGING_BUDDY!!! + 
        // НЕ ЗАБЫТЬ ПОМЕЧАТЬ ВСЕГДА СЕБЯ КАК ST_MERGING!!! + 
        // НЕ ЗАБЫТЬ ОБРАБОТАТЬ СИТУАЦИЮ, ЕСЛИ НАС СЛИЛ BUDDY, НО МЫ НЕ УСПЕЛИ УВИДЕТЬ, ЧТО ОН ST_MERGING!!! + 
    }

    return ib_push_free(cur_order, cur_start);
    // return ib_push_free(order, start);
}

bool free_chunk_mesh(uint chunk_id_local, uint chunk_id_global) {
    ChunkMeshAlloc a = chunk_alloc_global[chunk_id_global];
    bool r = true;
    if (a.v_startPage != INVALID_ID) {
        // uint vb_idx = atomicAdd(vb_alloc_stack_counter, 1u);
        // vb_alloc_stack[vb_idx] = uvec3(OP_FREE, a.v_startPage, a.v_order);

        // r = vb_free_pages(a.v_startPage, a.v_order);
    }

    if (a.i_startPage != INVALID_ID && r) {
        // uint ib_idx = atomicAdd(ib_alloc_stack_counter, 1u);
        // ib_alloc_stack[ib_idx] = uvec3(OP_FREE, a.i_startPage, a.i_order);

        r = ib_free_pages(a.i_startPage, a.i_order);
    }

    return r;
}

void main() {
    // if (gl_GlobalInvocationID.x != 0u) return;

    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    // for (uint dirtyIdx = 0; dirtyIdx < dirtyCount; dirtyIdx++) {
    if (pages_counters.count_vb_free_pages != INVALID_ID && pages_counters.count_ib_free_pages != INVALID_ID) {
        uint chunkId = dirty_list[dirtyIdx];

        if (chunk_alloc_global[chunkId].v_startPage != INVALID_ID && chunk_alloc_global[chunkId].v_startPage == chunk_alloc_local[dirtyIdx].v_startPage) {
            atomicAdd(stats[8], 1u);
        }

        if (chunk_alloc_global[chunkId].i_startPage != INVALID_ID && chunk_alloc_global[chunkId].i_startPage == chunk_alloc_local[dirtyIdx].i_startPage) {
            atomicAdd(stats[9], 1u);
        }

        bool r = free_chunk_mesh(dirtyIdx, chunkId);
        if (r)
            chunk_alloc_global[chunkId] = chunk_alloc_local[dirtyIdx];
    }
    // }
}
