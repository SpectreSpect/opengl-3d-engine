/*
План:
Основные правила, которые должны гарантироваться корректность работы структуры:
1. Любое изменение списка должн производиться "моментально" - так, чтобы после введения изменения 
результат сразу же был корректным и со списком можно было корректно взаимодействовать. Это необходимо
для исключения ситуаций, когда список находится в состоянии "изменение", во время которого использовать
его нельзя.
2. Из-за того, что для вставки страницы в список придётся использовать арбитраж, который занимает некоторое
время, страницу на этот промежуток времени необходимо помечать состоянием PUSHING, чтобы её не слили.

EDIT: Кажется это не нужно, ведь сливать страницы, которые находятся в списке свободных страниц *можно*, 
просто после этого их нужно пометить как MERGED, и !при pop выкидывать из списка свободных страниц!. MERGED
страницы в списке свободных страниц - это и есть stale страницы (протухшие), и это норм.
3. Проблемы ABA скорее всего не будет, так как арбитраж всегда быстрее, чем достать-положить-достатоть (в теории).


1. push(). Сначала необходимо сделать информацию, которую содержит страница полностью корректной, а только потом
за одну операцию вставить. Поэтому с самого начала помечаем её состоянием FREE - это может привести к тому, что
пока мы пытаемся вставить страницу в список, её могут слить и пометить состоянием MERGED, и, соответственно, наша
страница уже не будет по-настоящему свободной. Тем не менее, ничего в этом страшного нет - значит наша страница 
превратилась в stale страницу, а такие страницы спокойно умеют жить в списке, поэтому просто продолжаем вставку.
Чтобы вставить страницу, необходимо некоторое количество раз попытаться "успеть" это сделать, пока текущую вершину
никто другой не забрал. Для этого сначала считываем вершину списка, затем присоединяем эту вершину к вставляемой странице - 
так мы можем сделать с уверенностью того, что ничего не сломается, так как пока вставляемая страница не находится в списке, 
ни у кого не будет необходимости пытаться читать или изменять информацию о том, что к ней присоединено. Таким образом, на данном
этапе страница будет содержать полностью "корректную" информацию относительно списка, поэтому теперь мы должны попытаться её
вставить за одну опирацию записи - заменить номер страницы вершины списка на номер нашей вставляемой страницы. Производим
замену мы только в том случае, если до текущего момента на вершине списка находится до сих пор та же вершина, которую
мы записали себе как "следующую". Если это действительно так, просто производим запись и вставка будет на этом завершена.
В противном случае необходимо повторить попытку вставки заново - заново считать новую вершину, заново присоединить её к себе,
заново попытаться записать себя в вершину.
2. pop(). С помощью арбитража (CAS) некоторое количество раз пытаться взять страницу. Если получилось взять страницу,
то попытаться с помощью CAS заменить её состояние на ALLOC в случае, если её состояние сейчас FREE. В противном случае её
состояние может быть только MERGED, а значит это stale страница и её необходимо выкинуть, а затем продолжить арбитраж. Если
удалось заменить состояние на ALLOC, необходимо просто вернуть страницу как результат функции.

*/

#version 430
layout(local_size_x = 256) in;

#define INVALID_ID 0xFFFFFFFFu

// ---- existing ----
layout(std430, binding=5) buffer FrameCounters { uvec4 counters; }; // y = dirtyCount
layout(std430, binding=8) readonly buffer DirtyListBuf { uint dirty_list[]; };
layout(std430, binding=11) readonly buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };

layout(std430, binding=12) buffer EmitCounterBuf { uint emit_counter[]; };
layout(std430, binding=7)  buffer EnqueuedBuf    { uint enqueued[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

struct ChunkMeshMeta { uint first_index; uint index_count; uint base_vertex; uint mesh_valid; };
layout(std430, binding=9) buffer ChunkMeshMetaBuf { ChunkMeshMeta mesh_meta[]; };

// ---- NEW: per-chunk alloc info ----
layout(std430, binding=24) buffer ChunkMeshAllocBuf { uvec4 chunk_alloc[]; }; 
// x=v_startPage, y=v_order, z=i_startPage, w=i_order

// ---- NEW: VB pool ----
layout(std430, binding=18) coherent buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=19) coherent buffer VBNext  { uint vb_next[];  };
layout(std430, binding=20) coherent buffer VBState { uint vb_state[]; };

// ---- NEW: IB pool ----
layout(std430, binding=21) coherent buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=22) coherent buffer IBNext  { uint ib_next[];  };
layout(std430, binding=23) coherent buffer IBState { uint ib_state[]; };

layout(std430, binding=27) buffer AllocMarkers { uint vb_alloc_maker; uint ib_alloc_maker; };

layout(std430, binding=25) buffer DebugBuffer { uint stats[8]; uint allocator_stack_counter; uint allocator_stack[]; };

uniform uint u_vb_pages;
uniform uint u_ib_pages;
uniform uint u_vb_page_verts;  // например 256
uniform uint u_ib_page_inds;   // например 1024
uniform uint u_vb_max_order;   // log2(u_vb_pages)
uniform uint u_ib_max_order;   // log2(u_ib_pages)
uniform uint u_vb_index_bits;
uniform uint u_ib_index_bits;

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

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }

uint div_up_u32(uint a, uint b) { return (a + b - 1u) / b; }

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    // ceil_log2(x) = floor_log2(x-1) + 1
    return uint(findMSB(x - 1u) + 1);
}

uint vb_mask() { return (1u << u_vb_index_bits) - 1u; }

uint vb_pack_head(uint idx, uint tag) { return (tag << u_vb_index_bits) | idx; }
uint vb_head_idx(uint h) { return h & vb_mask(); }
uint vb_head_tag(uint h) { return h >> u_vb_index_bits; }


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
        if (atomicCompSwap(vb_heads[order], old_h, startPage) == old_h)
            return true; // Удалось вставить себя на вершину списка. Можем выходить из функции.
        
        // Кто-то первее нас изменил вершину списка. Повторяем попытку вставки снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
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
            return old_h; 
        }

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
}


uint vb_alloc_pages(uint wantOrder) {
    // Пытаемся достать страницу подходящего размера (2^wantOrder). Если страниц размера 2^wantOrder нет, 
    // то пытаемся достать страницу большего размера. Искать страницы меньшего размера не имеет смысла, потому
    // что наши данные в них не полезут (тк меньше чем 2^wantOrder), а объединить их не получится - функция
    // vb_free_pages сразу при освобождении страниц объединяет их, а если они не объеденены, значит сделать
    // этого было невозможно (необходимые страницы для объединения заняты ST_ALLOC).

    while (true) {
        // Показываем, что сейчас идёт аллокация
        atomicAdd(vb_alloc_maker, 1u);

        for (uint order = wantOrder; order <= u_vb_max_order; order++) {
            uint start_page = vb_pop_free(order); // Пытаемся достать страницу минимального размера
            if (start_page == INVALID_ID)
                continue; // Список страниц размера order пуст. Пытаемся найти страницу на новой итерации цикла.

            // Состояние страницы start_page сразу самой функций pop_free() назначается как ST_ALLOC.
            // Страницей владеем только мы, поэтому можем делать с ней что захотим без атомиков.

            // Теперь нужно поделить страницу до нужного нами размера (2^wantOrder), а остальное освободить.
            for (uint entire_order = order; entire_order > wantOrder; entire_order--) {
                uint half_page_size = 1u << (entire_order - 1u);
                uint start_buddy = start_page + half_page_size;

                atomicExchange(vb_state[start_buddy], pack_state(entire_order - 1, ST_MERGED));
                vb_push_free(entire_order - 1, start_buddy);

                // // Освобождаем страницу start_buddy
                // if (!vb_push_free(entire_order - 1, start_buddy)) {
                //     // Страницу вставил кто-то до нас. Но такого в теории быть не должно, потому что страницей 
                //     // владеем только мы (это обеспечивается функцией pop_free).
                //     atomicAdd(vb_alloc_maker, 0xFFFFFFFF); // Отнимаем единицу с помощью переполнения
                //     return INVALID_ID;
                // }
            }

            // Показываем, что аллокация завершена
            atomicAdd(vb_alloc_maker, 0xFFFFFFFF);
            
            // Теперь нужно сделать размер start_page равным wantOrder, так как он до сих пор равен order.
            // За одно поставили ST_ALLOC (тк так проще установить wantOrder), но в теории состояние и так уже 
            // должно было быть ST_ALLOC.
            vb_state[start_page] = pack_state(wantOrder, ST_ALLOC);
            return start_page;
        }

        atomicAdd(vb_alloc_maker, 0xFFFFFFFFu);

        if (atomicAdd(vb_alloc_maker, 0u) == 0u) {
            // Значит что ни один поток не выделяет память, но мы всё равно не смогли найти подходящий для нас размер
            // страницы. Такое может быть только в том случае, если память действительно закончилась.
            return INVALID_ID; 
        }

        // Какой-то поток сейчас выделяет страницы, значит он может нарезать кусочки, которые нам могут подойти, поэтому имеет
        // смысл попытаться выделить память снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём
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
    while (cur_order < u_vb_max_order) {
        memoryBarrierBuffer(); // На всякий случай.

        uint start_buddy = cur_start ^ (1u << cur_order);
        
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
            atomicExchange(vb_state[cur_start], pack_state(cur_order, ST_CONCEDED));
            return true;
        } else if ((buddy_kind == ST_MERGING) && buddy_order == cur_order) {
            // Значит кто-то уже сливает buddy. В этом случае, если 
            if (((cur_start >> cur_order) & 1u) == 1u) {
                // Мы справа, поэтому уступаем - слияние продолжит buddy вместо нас.
                
                // Но не уходим. Нужно убедиться, что buddy готов забирать память, а не решил сам уйти (если он
                // не успел увидеть, что мы тоже ST_MERGING, он может решить закончить слияние).
                continue;
            } else {
                // Мы слева - нам необходимо продолжить слияние. Это можно сделать только после того, как buddy
                // покажет, что он уступил (поставит состояние ST_CONCEDED). Поэтому пока ждём.

                // Помечаем себя как ST_READY, чтобы buddy мог спокойно отдать память и уйти.
                atomicExchange(vb_state[cur_start], pack_state(cur_order, ST_READY));
                we_are_ready = true;
                continue;
            }
        } else if ((buddy_kind == ST_FREE || buddy_kind == ST_CONCEDED) && buddy_order == cur_order) {
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
}

uint ib_mask() { return (1u << u_ib_index_bits) - 1u; }

uint ib_pack_head(uint idx, uint tag) { return (tag << u_ib_index_bits) | idx; }
uint ib_head_idx(uint h) { return h & ib_mask(); }
uint ib_head_tag(uint h) { return h >> u_ib_index_bits; }


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
        if (atomicCompSwap(ib_heads[order], old_h, startPage) == old_h)
            return true; // Удалось вставить себя на вершину списка. Можем выходить из функции.
        
        // Кто-то первее нас изменил вершину списка. Повторяем попытку вставки снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
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
            uint idx = atomicAdd(allocator_stack_counter, 2u);
            allocator_stack[idx] = old_h;
            allocator_stack[idx + 1u] = order;
            return old_h; 
        }

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
}


uint ib_alloc_pages(uint wantOrder) {
    // Пытаемся достать страницу подходящего размера (2^wantOrder). Если страниц размера 2^wantOrder нет, 
    // то пытаемся достать страницу большего размера. Искать страницы меньшего размера не имеет смысла, потому
    // что наши данные в них не полезут (тк меньше чем 2^wantOrder), а объединить их не получится - функция
    // ib_free_pages сразу при освобождении страниц объединяет их, а если они не объеденены, значит сделать
    // этого было невозможно (необходимые страницы для объединения заняты ST_ALLOC).

    while (true) {
        // Показываем, что сейчас идёт аллокация
        atomicAdd(ib_alloc_maker, 1u);

        for (uint order = wantOrder; order <= u_ib_max_order; order++) {
            uint start_page = ib_pop_free(order); // Пытаемся достать страницу минимального размера
            if (start_page == INVALID_ID)
                continue; // Список страниц размера order пуст. Пытаемся найти страницу на новой итерации цикла.

            // Состояние страницы start_page сразу самой функций pop_free() назначается как ST_ALLOC.
            // Страницей владеем только мы, поэтому можем делать с ней что захотим без атомиков.

            // Теперь нужно поделить страницу до нужного нами размера (2^wantOrder), а остальное освободить.
            for (uint entire_order = order; entire_order > wantOrder; entire_order--) {
                uint half_page_size = 1u << (entire_order - 1u);
                uint start_buddy = start_page + half_page_size;

                atomicExchange(ib_state[start_buddy], pack_state(entire_order - 1, ST_MERGED));
                ib_push_free(entire_order - 1, start_buddy);

                // // Освобождаем страницу start_buddy
                // if (!ib_push_free(entire_order - 1, start_buddy)) {
                //     // Страницу вставил кто-то до нас. Но такого в теории быть не должно, потому что страницей 
                //     // владеем только мы (это обеспечивается функцией pop_free).
                //     atomicAdd(ib_alloc_maker, 0xFFFFFFFF); // Отнимаем единицу с помощью переполнения
                //     return INVALID_ID;
                // }
            }

            // Показываем, что аллокация завершена
            atomicAdd(ib_alloc_maker, 0xFFFFFFFF);
            
            // Теперь нужно сделать размер start_page равным wantOrder, так как он до сих пор равен order.
            // За одно поставили ST_ALLOC (тк так проще установить wantOrder), но в теории состояние и так уже 
            // должно было быть ST_ALLOC.
            ib_state[start_page] = pack_state(wantOrder, ST_ALLOC);
            return start_page;
        }

        atomicAdd(ib_alloc_maker, 0xFFFFFFFFu);

        if (atomicAdd(ib_alloc_maker, 0u) == 0u) {
            // Значит что ни один поток не выделяет память, но мы всё равно не смогли найти подходящий для нас размер
            // страницы. Такое может быть только в том случае, если память действительно закончилась.
            return INVALID_ID; 
        }

        // Какой-то поток сейчас выделяет страницы, значит он может нарезать кусочки, которые нам могут подойти, поэтому имеет
        // смысл попытаться выделить память снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём
}

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
    while (cur_order < u_ib_max_order) {
        memoryBarrierBuffer(); // На всякий случай.

        uint start_buddy = cur_start ^ (1u << cur_order);
        
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
            atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_CONCEDED));
            return true;
        } else if ((buddy_kind == ST_MERGING) && buddy_order == cur_order) {
            // Значит кто-то уже сливает buddy. В этом случае, если 
            if (((cur_start >> cur_order) & 1u) == 1u) {
                // Мы справа, поэтому уступаем - слияние продолжит buddy вместо нас.
                
                // Но не уходим. Нужно убедиться, что buddy готов забирать память, а не решил сам уйти (если он
                // не успел увидеть, что мы тоже ST_MERGING, он может решить закончить слияние).
                continue;
            } else {
                // Мы слева - нам необходимо продолжить слияние. Это можно сделать только после того, как buddy
                // покажет, что он уступил (поставит состояние ST_CONCEDED). Поэтому пока ждём.

                // Помечаем себя как ST_READY, чтобы buddy мог спокойно отдать память и уйти.
                atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_READY));
                we_are_ready = true;
                continue;
            }
        } else if ((buddy_kind == ST_FREE || buddy_kind == ST_CONCEDED) && buddy_order == cur_order) {
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
}

void free_chunk_mesh(uint chunkId) {
    uvec4 a = chunk_alloc[chunkId];
    if (a.x != INVALID_ID) vb_free_pages(a.x, a.y);
    if (a.z != INVALID_ID) ib_free_pages(a.z, a.w);
    chunk_alloc[chunkId] = uvec4(INVALID_ID, 0u, INVALID_ID, 0u);
}

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;
    
    // uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.y;
    // if (dirtyIdx >= dirtyCount) return;

    for (uint dirtyIdx = 0; dirtyIdx < dirtyCount; dirtyIdx++) {
        uint chunkId = dirty_list[dirtyIdx];

        // мог быть уже выселен
        if (meta[chunkId].used == 0u) {
            // free_chunk_mesh(chunkId);
            mesh_meta[chunkId].mesh_valid  = 0u;
            mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            atomicAdd(stats[0], 1u);
            continue;
        }

        uint quads = dirty_quad_count[dirtyIdx];

        // пустой меш
        if (quads == 0u) {
            // free_chunk_mesh(chunkId);
            mesh_meta[chunkId].mesh_valid  = 0u;
            mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            atomicAdd(stats[1], 1u);
            continue;
        }

        // free старого
        // free_chunk_mesh(chunkId);

        uint needV = quads * 4u;
        uint needI = quads * 6u;

        uint vPages = div_up_u32(needV, u_vb_page_verts);
        uint iPages = div_up_u32(needI, u_ib_page_inds);

        uint vOrder = ceil_log2_u32(vPages);
        uint iOrder = ceil_log2_u32(iPages);

        uint vStart = vb_alloc_pages(vOrder);
        if (vStart == INVALID_ID) {
            // mesh_meta[chunkId].mesh_valid  = 0u;
            // mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            atomicAdd(stats[2], 1u);
            continue;
        }

        uint iStart = ib_alloc_pages(iOrder);
        if (iStart == INVALID_ID) {
            atomicExchange(stats[5], iOrder);
            atomicExchange(stats[6], iPages);
            atomicExchange(stats[7], quads);

            // vb_free_pages(vStart, vOrder); // rollback
            // mesh_meta[chunkId].mesh_valid  = 0u;
            // mesh_meta[chunkId].index_count = 0u;
            emit_counter[chunkId] = 0u;
            enqueued[chunkId] = 0u;
            atomicAdd(stats[3], 1u);
            continue;
        }

        chunk_alloc[chunkId] = uvec4(vStart, vOrder, iStart, iOrder);

        mesh_meta[chunkId].base_vertex = vStart * u_vb_page_verts;
        mesh_meta[chunkId].first_index = iStart * u_ib_page_inds;
        mesh_meta[chunkId].index_count = needI;
        mesh_meta[chunkId].mesh_valid  = 0u;

        emit_counter[chunkId] = 0u;
        enqueued[chunkId] = 0u;

        atomicAdd(stats[4], 1u);
    }
}
