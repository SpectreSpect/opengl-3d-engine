#version 430
layout(local_size_x = 256) in;

struct ChunkMeshAlloc {uint v_startPage; uint v_order; uint needV; uint i_startPage; uint i_order; uint needI; uint need_rebuild; };
layout(std430, binding=0) buffer GlobalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 

struct Node {uint page; uint next;};
// ---- NEW: VB pool ----
layout(std430, binding=1) coherent buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=2) coherent buffer VBState { uint vb_state[]; };
layout(std430, binding=3) coherent buffer VBNodes  { Node vb_nodes[];  };
layout(std430, binding=4) coherent buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=5) coherent buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };

// ---- NEW: IB pool ----
layout(std430, binding=6) coherent buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=7) coherent buffer IBState { uint ib_state[]; };
layout(std430, binding=8) coherent buffer IBNodes { Node ib_nodes[];  };
layout(std430, binding=9) coherent buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=10) coherent buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

layout(std430, binding=11) buffer EvictedChunksList { uint evicted_chunks_counter; uint evicted_chunks_list[]; };

uniform uint u_vb_max_order;
uniform uint u_ib_max_order;
uniform uint u_count_chunks_to_envict;

#define INVALID_ID 0xFFFFFFFFu

// ---- state packing ----
#define ST_MASK_BITS 4u
#define ST_FREE 0u
#define ST_ALLOC 1u
#define ST_MERGED 2u
#define ST_WAIT_TO_FREE 4u
const uint ST_MASK = (1u << ST_MASK_BITS) - 1u;

// ---- head state packing ----
#define HEAD_TAG_BITS 4u // Чтобы точно не случилось ABA, но если что можно уменьшить
const uint HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
const uint INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }
uint pack_head(uint start, uint tag) {return (start << HEAD_TAG_BITS) | (tag & HEAD_TAG_MASK); };
uint vb_pop_free_node_id() {
    for (;;) {
        uint old_counter = atomicAdd(vb_free_nodes_counter, 0u);
        if (old_counter == 0u) return INVALID_ID;
        
        if (atomicCompSwap(vb_free_nodes_counter, old_counter, old_counter - 1u) == old_counter) {
            memoryBarrierBuffer();
            return vb_free_nodes_list[old_counter - 1u];
        }
    }
}

void vb_push_free_node_id(uint node_id) {
    uint free_idx = atomicAdd(vb_returned_nodes_counter, 1u);
    vb_returned_nodes_list[free_idx] = node_id;
}

// При вставке состояние страницы не должно быть ST_FREE!!! Функция сама пометит страницу свободной.
bool vb_push_free(uint order, uint startPage)
{
    if (startPage == INVALID_ID) return false;

    uint node_id = vb_pop_free_node_id();
    if (node_id == INVALID_ID) return false;
    
    vb_nodes[node_id].page = startPage;

    atomicExchange(vb_state[startPage], pack_state(order, ST_FREE));

    for (;;)
    {
        uint old_h = atomicAdd(vb_heads[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        vb_nodes[node_id].next = old_idx == INVALID_HEAD_IDX ? INVALID_ID : old_idx;

        memoryBarrierBuffer();

        uint new_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(node_id, new_tag);

        if (atomicCompSwap(vb_heads[order], old_h, new_head) == old_h)
            return true;
    }
}

uint vb_pop_free(uint order) {
    for (;;) {
        uint old_h = atomicAdd(vb_heads[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        if (old_idx == INVALID_HEAD_IDX)
            return INVALID_ID; // Список пустой - выходим

        memoryBarrierBuffer(); // Чтобы old_h было ДО next

        uint next = atomicAdd(vb_nodes[old_idx].next, 0u);
        uint new_head_idx = next == INVALID_ID ? INVALID_HEAD_IDX : next;
        uint new_head_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(new_head_idx, new_head_tag);

        if (atomicCompSwap(vb_heads[order], old_h, new_head) != old_h) continue;

        memoryBarrierBuffer();
        
        uint old_head_page = vb_nodes[old_idx].page; // Нод доступен только нам, поэтому, в теории, чтение безопасно

        vb_push_free_node_id(old_idx);

        uint free_state = pack_state(order, ST_FREE);
        if (atomicCompSwap(vb_state[old_head_page], free_state, pack_state(order, ST_ALLOC)) == free_state) {
            return old_head_page;
        }

        

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return INVALID_ID;
}

//Освобождать можно только уже выделенную память (та, что имеет состояние ST_ALLOC)!!! Иначе работать не будет!
bool vb_free_pages(uint start, uint order) {
    if (start == INVALID_ID) return false;
    if (order > u_vb_max_order) return false;

    // Проверка, что освобождаемая память является занятой (ST_ALLOC) и также попытка забрать владение на освобожение
    uint alloc_state = pack_state(order, ST_ALLOC);
    uint prev_input_start_state = atomicCompSwap(vb_state[start], alloc_state, pack_state(order, ST_MERGED)); 
    if (prev_input_start_state != alloc_state) {
        // Либо кто-то раньше нас начал освобождать, либо память не была ST_ALLOC
        return false;
    }

    uint merge_id = 0u;
    uint cur_start = start;
    uint cur_order = order;
    while (cur_order < u_vb_max_order) {
        memoryBarrierBuffer(); // На всякий случай.

        uint buddy_size = 1u << cur_order;
        uint start_buddy = cur_start ^ buddy_size;

        if (start_buddy + buddy_size > (1u << u_vb_max_order)) break;
        
        uint buddy_state = atomicAdd(vb_state[start_buddy], 0u);

        uint buddy_kind = buddy_state & ST_MASK;
        uint buddy_order = buddy_state >> ST_MASK_BITS;

        if(buddy_kind != ST_FREE || buddy_order != cur_order) break;

        // Здесь buddy является ST_FREE
        uint free_state = pack_state(buddy_order, ST_FREE);
        if (atomicCompSwap(vb_state[start_buddy], free_state, pack_state(buddy_order, ST_MERGED)) != free_state) {
            // Кто-то первее забрал buddy
            break;
        }

        cur_start = min(cur_start, start_buddy);
        cur_order++;
        merge_id++;
    }
    
    uint state_before_change = atomicExchange(vb_state[cur_start], pack_state(cur_order, ST_MERGED));
    bool result = vb_push_free(cur_order, cur_start);
    return result;
}

uint ib_pop_free_node_id() {
    for (;;) {
        uint old_counter = atomicAdd(ib_free_nodes_counter, 0u);
        if (old_counter == 0u) return INVALID_ID;
        
        if (atomicCompSwap(ib_free_nodes_counter, old_counter, old_counter - 1u) == old_counter) {
            memoryBarrierBuffer();
            return ib_free_nodes_list[old_counter - 1u];
        }
    }
}

void ib_push_free_node_id(uint node_id) {
    uint free_idx = atomicAdd(ib_returned_nodes_counter, 1u);
    ib_returned_nodes_list[free_idx] = node_id;
}

// При вставке состояние страницы не должно быть ST_FREE!!! Функция сама пометит страницу свободной.
bool ib_push_free(uint order, uint startPage)
{
    if (startPage == INVALID_ID) return false;

    uint node_id = ib_pop_free_node_id();
    if (node_id == INVALID_ID) return false;
    
    ib_nodes[node_id].page = startPage;

    atomicExchange(ib_state[startPage], pack_state(order, ST_FREE));

    for (;;)
    {
        uint old_h = atomicAdd(ib_heads[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        ib_nodes[node_id].next = old_idx == INVALID_HEAD_IDX ? INVALID_ID : old_idx;

        memoryBarrierBuffer();

        uint new_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(node_id, new_tag);

        if (atomicCompSwap(ib_heads[order], old_h, new_head) == old_h)
            return true;
    }
}

uint ib_pop_free(uint order) {
    for (;;) {
        uint old_h = atomicAdd(ib_heads[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        if (old_idx == INVALID_HEAD_IDX)
            return INVALID_ID; // Список пустой - выходим

        memoryBarrierBuffer(); // Чтобы old_h было ДО next

        uint next = atomicAdd(ib_nodes[old_idx].next, 0u);
        uint new_head_idx = next == INVALID_ID ? INVALID_HEAD_IDX : next;
        uint new_head_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(new_head_idx, new_head_tag);

        if (atomicCompSwap(ib_heads[order], old_h, new_head) != old_h) continue;

        memoryBarrierBuffer();
        
        uint old_head_page = ib_nodes[old_idx].page; // Нод доступен только нам, поэтому, в теории, чтение безопасно

        ib_push_free_node_id(old_idx);

        uint free_state = pack_state(order, ST_FREE);
        if (atomicCompSwap(ib_state[old_head_page], free_state, pack_state(order, ST_ALLOC)) == free_state) {
            return old_head_page;
        }

        

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return INVALID_ID;
}

//Освобождать можно только уже выделенную память (та, что имеет состояние ST_ALLOC)!!! Иначе работать не будет!
bool ib_free_pages(uint start, uint order) {
    if (start == INVALID_ID) return false;
    if (order > u_ib_max_order) return false;

    // Проверка, что освобождаемая память является занятой (ST_ALLOC) и также попытка забрать владение на освобожение
    uint alloc_state = pack_state(order, ST_ALLOC);
    uint prev_input_start_state = atomicCompSwap(ib_state[start], alloc_state, pack_state(order, ST_MERGED)); 
    if (prev_input_start_state != alloc_state) {
        // Либо кто-то раньше нас начал освобождать, либо память не была ST_ALLOC
        return false;
    }

    uint merge_id = 0u;
    uint cur_start = start;
    uint cur_order = order;
    while (cur_order < u_ib_max_order) {
        memoryBarrierBuffer(); // На всякий случай.

        uint buddy_size = 1u << cur_order;
        uint start_buddy = cur_start ^ buddy_size;

        if (start_buddy + buddy_size > (1u << u_ib_max_order)) break;
        
        uint buddy_state = atomicAdd(ib_state[start_buddy], 0u);

        uint buddy_kind = buddy_state & ST_MASK;
        uint buddy_order = buddy_state >> ST_MASK_BITS;

        if(buddy_kind != ST_FREE || buddy_order != cur_order) break;

        // Здесь buddy является ST_FREE
        uint free_state = pack_state(buddy_order, ST_FREE);
        if (atomicCompSwap(ib_state[start_buddy], free_state, pack_state(buddy_order, ST_MERGED)) != free_state) {
            // Кто-то первее забрал buddy
            break;
        }

        cur_start = min(cur_start, start_buddy);
        cur_order++;
        merge_id++;
    }
    
    uint state_before_change = atomicExchange(ib_state[cur_start], pack_state(cur_order, ST_MERGED));
    bool result = ib_push_free(cur_order, cur_start);
    return result;
}

void main() {
    if (gl_GlobalInvocationID.x == 0u) evicted_chunks_counter = 0u;

    uint envicted_list_id = gl_GlobalInvocationID.x;
    if (envicted_list_id >= u_count_chunks_to_envict) return;

    uint chunk_id = evicted_chunks_list[envicted_list_id];
    if (chunk_id == INVALID_ID) return;

    // Отчистка памяти
    ChunkMeshAlloc chunk_alloc = chunk_alloc_global[chunk_id];
    if (chunk_alloc.v_startPage != INVALID_ID) vb_free_pages(chunk_alloc.v_startPage, chunk_alloc.v_order);
    if (chunk_alloc.i_startPage != INVALID_ID) vb_free_pages(chunk_alloc.i_startPage, chunk_alloc.i_order);

    // Запись информации о том, что память для меша чанка chunk_id не выделенна
    chunk_alloc_global[chunk_id].v_startPage = INVALID_ID;
    chunk_alloc_global[chunk_id].v_order = 0u;
    chunk_alloc_global[chunk_id].needV = 0u;
    chunk_alloc_global[chunk_id].i_startPage = INVALID_ID;
    chunk_alloc_global[chunk_id].i_order = 0u;
    chunk_alloc_global[chunk_id].needI = 0u;
    chunk_alloc_global[chunk_id].need_rebuild = 0u;
}