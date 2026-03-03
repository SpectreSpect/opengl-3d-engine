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

// ---- existing ----
struct FrameCounters {
    uint write_count; 
    uint dirty_count;
    uint cmd_count;
    uint free_count;
    uint failed_dirty_count;
    uint count_vb_free_pages;
    uint count_ib_free_pages;
};
layout(std430, binding=0) buffer FrameCountersBuf { FrameCounters counters; }; // y = dirtyCount
layout(std430, binding=1) readonly buffer DirtyListBuf { uint dirty_list[]; };
layout(std430, binding=2) readonly buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };

struct ChunkMeta { uint used; uint key_lo; uint key_hi; uint dirty_flags; };
layout(std430, binding=3) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

// ---- NEW: per-chunk alloc info ----
struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=4) buffer ChunkMeshAllocLocalBuf { ChunkMeshAlloc chunk_alloc_local[]; }; 
layout(std430, binding=5) readonly buffer ChunkMeshAllocGlobalBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 

// ---- NEW: BB pool ----
struct Node {uint page; uint next;};

layout(std430, binding=6) coherent buffer BBHeads { uint bb_heads[]; };
layout(std430, binding=7) coherent buffer BBState { uint bb_state[]; };
layout(std430, binding=8) coherent buffer BBNodes  { Node bb_nodes[];  };
layout(std430, binding=9) coherent buffer BBFreeNodesList  { uint bb_free_nodes_counter; uint bb_free_nodes_list[];  };
layout(std430, binding=10) coherent buffer BBReturnedNodesList  { uint bb_returned_nodes_counter; uint bb_returned_nodes_list[]; };

layout(std430, binding=11) buffer DebugBuffer { uint stats[]; };

uniform uint u_bb_pages;
uniform uint u_bb_page_elements;  // например 256
uniform uint u_bb_max_order;   // log2(u_bb_pages)
uniform uint u_bb_quad_size;
uniform uint u_is_vb_phase;

bool is_vb_phase = u_is_vb_phase == 1u;

#define INVALID_ID 0xFFFFFFFFu

// ---- state packing ----
#define ST_MASK_BITS 4u
#define ST_FREE 0u
#define ST_ALLOC 1u
#define ST_MERGED 2u
const uint ST_MASK = (1u << ST_MASK_BITS) - 1u;

// ---- head state packing ----
#define HEAD_TAG_BITS 4u // Чтобы точно не случилось ABA, но если что можно уменьшить
const uint HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
const uint INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;

const uint OP_ALLOC = 0u;
const uint OP_FREE = 1u;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); };
uint pack_head(uint start, uint tag) {return (start << HEAD_TAG_BITS) | (tag & HEAD_TAG_MASK); };

uint div_up_u32(uint a, uint b) { return (a + b - 1u) / b; }

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    // ceil_log2(x) = floor_log2(x-1) + 1
    return uint(findMSB(x - 1u) + 1);
}

uint bb_pop_free_node_id() {
    for (;;) {
        uint old_counter = atomicAdd(bb_free_nodes_counter, 0u);
        if (old_counter == 0u) return INVALID_ID;
        
        if (atomicCompSwap(bb_free_nodes_counter, old_counter, old_counter - 1u) == old_counter) {
            memoryBarrierBuffer();
            return bb_free_nodes_list[old_counter - 1u];
        }
    }
}

void bb_push_free_node_id(uint node_id) {
    uint free_idx = atomicAdd(bb_returned_nodes_counter, 1u);
    bb_returned_nodes_list[free_idx] = node_id;
}

// При вставке состояние страницы не должно быть ST_FREE!!! Функция сама пометит страницу свободной.
bool bb_push_free(uint order, uint startPage)
{
    if (startPage == INVALID_ID) return false;

    uint node_id = bb_pop_free_node_id();
    if (node_id == INVALID_ID) return false;
    
    bb_nodes[node_id].page = startPage;

    atomicExchange(bb_state[startPage], pack_state(order, ST_FREE));

    // for (;;)
    // {
    //     uint prev_state = atomicAdd(bb_state[startPage], 0u);
    //     uint new_state = pack_state(order, ST_FREE);
    //     if (prev_state == new_state) {
    //         bb_push_free_node_id(node_id);
    //         return true; // Уже в списке 
    //     }

    //     if (atomicCompSwap(bb_state[startPage], prev_state, new_state) == prev_state)
    //         break;
    // }

    for (;;)
    {
        uint old_h = atomicAdd(bb_heads[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        bb_nodes[node_id].next = old_idx == INVALID_HEAD_IDX ? INVALID_ID : old_idx;

        memoryBarrierBuffer();

        uint new_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(node_id, new_tag);

        if (atomicCompSwap(bb_heads[order], old_h, new_head) == old_h)
            return true;
    }
}

uint bb_pop_free(uint order) {
    for (;;) {
        uint old_h = atomicAdd(bb_heads[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        if (old_idx == INVALID_HEAD_IDX)
            return INVALID_ID; // Список пустой - выходим

        memoryBarrierBuffer(); // Чтобы old_h было ДО next

        uint next = atomicAdd(bb_nodes[old_idx].next, 0u);
        uint new_head_idx = next == INVALID_ID ? INVALID_HEAD_IDX : next;
        uint new_head_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(new_head_idx, new_head_tag);

        if (atomicCompSwap(bb_heads[order], old_h, new_head) != old_h) continue;

        memoryBarrierBuffer();
        
        uint old_head_page = bb_nodes[old_idx].page; // Нод доступен только нам, поэтому, в теории, чтение безопасно

        bb_push_free_node_id(old_idx);

        uint free_state = pack_state(order, ST_FREE);
        if (atomicCompSwap(bb_state[old_head_page], free_state, pack_state(order, ST_ALLOC)) == free_state) {
            return old_head_page;
        }

        

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return INVALID_ID;
}

uint bb_alloc_pages(uint wantOrder) {
    // Пытаемся достать страницу подходящего размера (2^wantOrder). Если страниц размера 2^wantOrder нет, 
    // то пытаемся достать страницу большего размера. Искать страницы меньшего размера не имеет смысла, потому
    // что наши данные в них не полезут (тк меньше чем 2^wantOrder), а объединить их не получится - функция
    // bb_free_pages сразу при освобождении страниц объединяет их, а если они не объеденены, значит сделать
    // этого было невозможно (необходимые страницы для объединения заняты ST_ALLOC).

    uint attempt = 0u;
    while (attempt < 32u) {
        // Показываем, что сейчас идёт аллокация
        // atomicAdd(bb_alloc_maker, 1u);

        for (uint order = wantOrder; order <= u_bb_max_order; order++) {
            uint start_page = bb_pop_free(order); // Пытаемся достать страницу минимального размера
            if (start_page == INVALID_ID)
                continue; // Список страниц размера order пуст. Пытаемся найти страницу на новой итерации цикла.

            // Состояние страницы start_page сразу самой функций pop_free() назначается как ST_ALLOC.
            // Страницей владеем только мы, поэтому можем делать с ней что захотим без атомиков.

            // Теперь нужно поделить страницу до нужного нами размера (2^wantOrder), а остальное освободить.
            for (uint entire_order = order; entire_order > wantOrder; entire_order--) {
                uint half_page_size = 1u << (entire_order - 1u);
                uint start_buddy = start_page + half_page_size;

                atomicExchange(bb_state[start_buddy], pack_state(entire_order - 1, ST_MERGED));
                bb_push_free(entire_order - 1, start_buddy);

                // // Освобождаем страницу start_buddy
                // if (!bb_push_free(entire_order - 1, start_buddy)) {
                //     // Страницу вставил кто-то до нас. Но такого в теории быть не должно, потому что страницей 
                //     // владеем только мы (это обеспечивается функцией pop_free).
                //     atomicAdd(bb_alloc_maker, 0xFFFFFFFF); // Отнимаем единицу с помощью переполнения
                //     return INVALID_ID;
                // }
            }

            // Показываем, что аллокация завершена
            // atomicAdd(bb_alloc_maker, 0xFFFFFFFF);
            
            // Теперь нужно сделать размер start_page равным wantOrder, так как он до сих пор равен order.
            // За одно поставили ST_ALLOC (тк так проще установить wantOrder), но в теории состояние и так уже 
            // должно было быть ST_ALLOC.
            bb_state[start_page] = pack_state(wantOrder, ST_ALLOC);
            return start_page;
        }

        // atomicAdd(bb_alloc_maker, 0xFFFFFFFFu);

        // if (atomicAdd(bb_alloc_maker, 0u) == 0u) {
        //     // Значит что ни один поток не выделяет память, но мы всё равно не смогли найти подходящий для нас размер
        //     // страницы. Такое может быть только в том случае, если память действительно закончилась.
        //     return INVALID_ID; 
        // }

        // Какой-то поток сейчас выделяет страницы, значит он может нарезать кусочки, которые нам могут подойти, поэтому имеет
        // смысл попытаться выделить память снова.
        attempt++;
    }

    return INVALID_ID;
    // Цикл бесконечный, поэтому сюда никогда не попадём
}

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;
    
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = counters.dirty_count;
    if (dirtyIdx >= dirtyCount) return;



    // uint order = 15u;
    // uint old_head = bb_heads[order];
    // stats[0] = old_head;
    // stats[1] = old_head & HEAD_TAG_MASK;
    // stats[2] = old_head >> HEAD_TAG_BITS;
    // stats[15] = bb_nodes[old_head >> HEAD_TAG_BITS].page;
    // stats[16] = bb_nodes[old_head >> HEAD_TAG_BITS].next;

    // uint head_id = bb_pop_free(order);

    // stats[3] = head_id;
    // uint after_pop_head = bb_heads[order];
    // stats[4] = after_pop_head;
    // stats[5] = after_pop_head & HEAD_TAG_MASK;
    // stats[6] = after_pop_head >> HEAD_TAG_BITS;

    // uint new_order = 14u;
    // stats[7] = bb_push_free(new_order, head_id) ? 1u : 0u;
    
    // uint head_after_push = bb_heads[new_order];
    // stats[8] = head_after_push;
    // stats[9] = head_after_push & HEAD_TAG_MASK;
    // stats[10] = head_after_push >> HEAD_TAG_BITS;

    // uint new_order_head_idx = bb_pop_free(new_order);
    // stats[11] = new_order_head_idx;
    
    // uint head_after_last_pop = bb_heads[new_order];
    // stats[12] = head_after_last_pop;
    // stats[13] = head_after_last_pop & HEAD_TAG_MASK;
    // stats[14] = head_after_last_pop >> HEAD_TAG_BITS;


    for (uint dirtyIdx = 0; dirtyIdx < dirtyCount; dirtyIdx++) {
    uint chunkId = dirty_list[dirtyIdx];

    if (counters.count_vb_free_pages == INVALID_ID || counters.count_ib_free_pages == INVALID_ID) {
        if (is_vb_phase){
            counters.count_vb_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            counters.count_ib_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }

        chunk_alloc_local[dirtyIdx].need_rebuild = 1u;
        continue;
    }

    // мог быть уже выселен
    if (meta[chunkId].used == 0u) {
        if (is_vb_phase){
            counters.count_vb_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            counters.count_ib_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }
        
        chunk_alloc_local[dirtyIdx].need_rebuild = 1u;
        continue;
    }

    uint quads = dirty_quad_count[dirtyIdx];

    // пустой меш
    if (quads == 0u) {
        if (is_vb_phase){
            counters.count_vb_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            counters.count_ib_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }
        
        chunk_alloc_local[dirtyIdx].need_rebuild = 1u;
        continue;
    }

    uint needB = quads * u_bb_quad_size;
    uint bPages = div_up_u32(needB, u_bb_page_elements);
    uint bOrder = ceil_log2_u32(bPages);

    uint bStart = bb_alloc_pages(bOrder);
    if (bStart == INVALID_ID) {
        if (is_vb_phase){
            counters.count_vb_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            counters.count_ib_free_pages = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }
        
        chunk_alloc_local[dirtyIdx].need_rebuild = 1u;
        continue;
    }

    if (is_vb_phase) {
        chunk_alloc_local[dirtyIdx].v_startPage = bStart;
        chunk_alloc_local[dirtyIdx].v_order = bOrder;
        chunk_alloc_local[dirtyIdx].needV = needB;
        chunk_alloc_local[dirtyIdx].need_rebuild = 1u;
    } else {
        chunk_alloc_local[dirtyIdx].i_startPage = bStart;
        chunk_alloc_local[dirtyIdx].i_order = bOrder;
        chunk_alloc_local[dirtyIdx].needI = needB;
        chunk_alloc_local[dirtyIdx].need_rebuild = 1u;
    }

    }
}
