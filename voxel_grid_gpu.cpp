#include "voxel_grid_gpu.h"

VoxelGridGPU::VoxelGridGPU(
    glm::ivec3 chunk_size, 
    glm::vec3 voxel_size, 
    uint32_t count_active_chunks,
    uint32_t max_quads, 
    float chunk_hash_table_size_factor,
    uint32_t max_count_probing,
    uint32_t count_evict_buckets,
    uint32_t min_free_chunks,
    uint32_t max_evict_chunks,
    uint32_t bucket_step,
    uint32_t vb_page_size_order_of_two,
    uint32_t ib_page_size_order_of_two,
    ShaderManager& shader_manager
) {
    assert(chunk_hash_table_size_factor >= 1.0f);

    this->chunk_size = chunk_size;
    this->voxel_size = voxel_size;
    this->count_active_chunks = count_active_chunks;
    this->max_count_probing = max_count_probing;
    this->count_evict_buckets = count_evict_buckets;
    this->min_free_chunks = min_free_chunks;
    this->max_evict_chunks = max_evict_chunks;
    this->bucket_step = bucket_step;

    uint64_t raw = (uint64_t)std::ceil((double)chunk_hash_table_size_factor * (double)count_active_chunks);
    uint32_t base = (raw > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw;
    this->chunk_hash_table_size = math_utils::next_pow2_u32(base);
    assert((chunk_hash_table_size & (chunk_hash_table_size - 1)) == 0);


    init_programs(shader_manager);

    chunk_meta_ = SSBO(sizeof(ChunkMetaGPU) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    free_list_ = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    active_list_ = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW); 
    indirect_cmds_ = SSBO(sizeof(DrawElementsIndirectCommand) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    
    // uint32_t frame_counters_u[4] = {
    //     0, //voxel_write_count
    //     0, //dirty_count
    //     0, //cmd_count
    //     count_active_chunks //free_count
    // };

    uint32_t frame_counters_u[5] = {0}; 
    frame_counters_ = SSBO(sizeof(uint32_t) * 5, GL_DYNAMIC_DRAW, frame_counters_u);
    count_free_pages_ = SSBO(sizeof(uint32_t) * 2, GL_DYNAMIC_DRAW);

    equeued_ = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    dirty_list_ = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    failed_dirty_list_ = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    chunk_meta_      = SSBO(sizeof(ChunkMetaGPU) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    dirty_quad_count_ = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    emit_counter_     = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    mesh_counters_    = SSBO(sizeof(uint32_t) * 2, GL_DYNAMIC_DRAW); // uvec2

    bucket_heads_ = SSBO(sizeof(uint32_t) * count_evict_buckets, GL_DYNAMIC_DRAW);
    bucket_next_  = SSBO(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW);

    // Глобальные буферы (выбери разумный budget!)
    // Например 2M quads per frame => 8M verts, 12M indices
    vb_page_size_ = 1 << vb_page_size_order_of_two;
    count_vb_pages_ = math_utils::next_pow2_u32(math_utils::div_up_u32((max_quads * 4u), vb_page_size_));
    vb_index_bits_ = math_utils::log2_ceil_u32(count_vb_pages_ + 1); // + 1 для значения INVALID_VALUE
    vb_order_ = math_utils::log2_floor_u32(count_vb_pages_);
    max_mesh_vertices_ = count_vb_pages_ * vb_page_size_;
    
    std::cout << "vb_page_size_: " << vb_page_size_ << std::endl;
    std::cout << "count_vb_pages_: " << count_vb_pages_ << std::endl;
    std::cout << "vb_index_bits_: " << vb_index_bits_ << std::endl;
    std::cout << "vb_order_: " << vb_order_ << std::endl;
    std::cout << "max_mesh_vertices_: " << max_mesh_vertices_ << std::endl;
    std::cout << std::endl;

    global_vertex_buffer_ = SSBO(sizeof(VertexGPU) * (size_t)max_mesh_vertices_, GL_DYNAMIC_DRAW);
    vb_heads_ = SSBO(sizeof(uint32_t) * (size_t)(vb_order_ + 1), GL_DYNAMIC_DRAW);
    vb_next_ = SSBO(sizeof(uint32_t) * (size_t)count_vb_pages_, GL_DYNAMIC_DRAW);
    vb_state_ = SSBO(sizeof(uint32_t) * (size_t)count_vb_pages_, GL_DYNAMIC_DRAW);

    ib_page_size_ = 1 << ib_page_size_order_of_two;
    count_ib_pages_ = math_utils::next_pow2_u32(math_utils::div_up_u32((max_quads * 6u), ib_page_size_));
    ib_index_bits_ = math_utils::log2_ceil_u32(count_ib_pages_ + 1); // + 1 для значения INVALID_VALUE
    ib_order_ = math_utils::log2_floor_u32(count_ib_pages_);
    max_mesh_indices_ = count_ib_pages_ * ib_page_size_;

    std::cout << "ib_page_size_: " << ib_page_size_ << std::endl;
    std::cout << "count_ib_pages_: " << count_ib_pages_ << std::endl;
    std::cout << "ib_index_bits_: " << ib_index_bits_ << std::endl;
    std::cout << "ib_order_: " << ib_order_ << std::endl;
    std::cout << "max_mesh_indices_: " << max_mesh_indices_ << std::endl;
    std::cout << std::endl;
    
    global_index_buffer_ = SSBO(sizeof(uint32_t) * (size_t)max_mesh_indices_, GL_DYNAMIC_DRAW);
    ib_heads_ = SSBO(sizeof(uint32_t) * (size_t)(ib_order_ + 1), GL_DYNAMIC_DRAW);
    ib_next_ = SSBO(sizeof(uint32_t) * (size_t)count_ib_pages_, GL_DYNAMIC_DRAW);
    ib_state_ = SSBO(sizeof(uint32_t) * (size_t)count_ib_pages_, GL_DYNAMIC_DRAW);

    chunk_mesh_alloc_ = SSBO(sizeof(ChunkMeshAlloc) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    chunk_mesh_alloc_local_ = SSBO(sizeof(ChunkMeshAlloc) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    voxel_prifab_ = SSBO(sizeof(VoxelDataGPU), GL_DYNAMIC_DRAW);

    stream_counters_ = SSBO(sizeof(uint32_t) * 2, GL_DYNAMIC_DRAW);
    load_list_       = SSBO(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    uint32_t debug_init_values[5000] = {};
    debug_counters_ = SSBO(sizeof(uint32_t) * (size_t)5000, GL_DYNAMIC_DRAW, debug_init_values);

    uint32_t v[2] = {0};
    stream_generate_debug_counters_ = SSBO(sizeof(uint32_t) * (size_t)2, GL_DYNAMIC_DRAW, v);

    uint32_t alloc_marker_value[2] = {0};
    alloc_markers_ = SSBO(sizeof(uint32_t), GL_DYNAMIC_DRAW, alloc_marker_value);

    VoxelDataGPU init_chunk_voxel_prifab = {0};
    init_chunk_voxel_prifab.type_vis_flags |= (0u & 0xFFu) << 16; // Тип 0
    init_chunk_voxel_prifab.type_vis_flags |= (0u & 0xFFu) << 8; // Прозрачный
    init_chunk_voxel_prifab.color = 0xffffffff; // Белый
    init_active_chunks(chunk_size, count_active_chunks, init_chunk_voxel_prifab);
    init_chunks_hash_table();
    world_init_gpu();
    init_draw_buffers();
    init_mesh_pool();
    load_alloc_stacks();
}

void VoxelGridGPU::draw(RenderState state) {
    state.transform *= get_model_matrix();

    build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);
    build_indirect_draw_commands_frustum(state.vp, math_utils::BITS, math_utils::OFFSET);
    draw_indirect(vao.id, state.transform, state.vp, state.camera->position);

    // uint32_t old_dirty_count = 8;
    // std::vector<uint32_t> dirty_list(old_dirty_count);
    // dirty_list_.read_subdata(0, dirty_list.data(), old_dirty_count * sizeof(uint32_t));

    // std::cout << "dirty_list: ";
    // for (int i = 0; i < old_dirty_count; i++)  {
    //     std::cout << dirty_list[i];
    //     if (i != old_dirty_count - 1) std::cout << ", ";
    // }
    // std::cout << std::endl;

    // uint32_t count_voxels_in_chunk = chunk_size.x * chunk_size.y * chunk_size.z;
    // uint32_t count_visible = 0;
    // for (uint32_t i = 0; i < old_dirty_count; i++) {
    //     uint32_t chunk_id = dirty_list[i];
    //     size_t chunk_base = chunk_id * count_voxels_in_chunk;

    //     std::vector<VoxelDataGPU> voxels(count_voxels_in_chunk);
    //     voxels_.read_subdata(chunk_base * sizeof(VoxelDataGPU), voxels.data(), count_voxels_in_chunk * sizeof(VoxelDataGPU));

    //     for (VoxelDataGPU& v : voxels) {
    //         uint32_t type = (v.type_vis_flags >> 16) & 0xFFu;
    //         if (type != 0) count_visible++;
    //     }
    // }

    // std::cout << "count_visible: " << count_visible << std::endl;
}

void VoxelGridGPU::print_counters(uint32_t write_count, uint32_t dirty_count, uint32_t cmd_count, uint32_t free_count, uint32_t load_list_count) {
    uint32_t counters[4] = {0};
    frame_counters_.read_subdata(0, counters, sizeof(uint32_t) * 4);

    uint32_t stream_counters[2] = {0};
    stream_counters_.read_subdata(0, stream_counters, sizeof(uint32_t) * 2);

    if (write_count != 0) std::cout << "write_count: " << counters[0] << std::endl;
    if (dirty_count != 0) std::cout << "dirty_count: " << counters[1] << std::endl;
    if (cmd_count != 0) std::cout << "cmd_count: " << counters[2] << std::endl;
    if (free_count != 0) std::cout << "free_count: " << counters[3] << std::endl;
    if (load_list_count != 0) std::cout << "load_list_count: " << stream_counters[0] << std::endl;
    std::cout << std::endl;

    print_count_free_mesh_alloc();
}

void VoxelGridGPU::print_count_free_mesh_alloc() {
    std::vector<ChunkMeshAlloc> alloc_meta(count_active_chunks);
    chunk_mesh_alloc_.read_subdata(0, alloc_meta.data(), sizeof(ChunkMeshAlloc) * count_active_chunks);

    std::vector<uint32_t> vb_states(count_vb_pages_);
    vb_state_.read_subdata(0, vb_states.data(), sizeof(uint32_t) * count_vb_pages_);

    std::vector<uint32_t> ib_states(count_ib_pages_);
    ib_state_.read_subdata(0, ib_states.data(), sizeof(uint32_t) * count_ib_pages_);

    #define INVALID_ID 0xFFFFFFFFu

    std::unordered_set<uint32_t> allocated_mesh;
    uint32_t count_alloc_vb_pages_from_meta = 0, count_alloc_ib_pages_from_meta = 0;
    uint32_t count_vb_alloc_chunks_from_meta = 0, count_ib_alloc_chunks_from_meta = 0;
    for (uint32_t i = 0; i < count_active_chunks; i++) {
        ChunkMeshAlloc& meta = alloc_meta[i];
        if (meta.v_startPage != INVALID_ID) {
            count_alloc_vb_pages_from_meta += 1 << meta.v_order;
            count_vb_alloc_chunks_from_meta++;
            allocated_mesh.insert(i);
        }

        if (meta.i_startPage != INVALID_ID) {
            allocated_mesh.insert(i);
            // std::cout << "ORDER: " << meta.i_order << std::endl;
            // std::cout << "2^ " << meta.i_order << ": " << (1 << meta.i_order) << std::endl;
            // std::cout << "i_start: " << meta.i_startPage << std::endl;
            count_alloc_ib_pages_from_meta += 1 << meta.i_order;
            count_ib_alloc_chunks_from_meta++;
        }
    }

    //=================РАСЧЁТ ПЕРЕСЕЧЕНИЙ=================
    std::vector<uint32_t> ids(allocated_mesh.begin(), allocated_mesh.end());
    std::vector<std::pair<uint32_t,uint32_t>> v_pairs, i_pairs;
    v_pairs.reserve(64); i_pairs.reserve(64);

    for (size_t a = 0; a < ids.size(); ++a) {
        for (size_t b = a + 1; b < ids.size(); ++b) {
            uint32_t ida = ids[a], idb = ids[b];
            const auto &A = alloc_meta[ida];
            const auto &B = alloc_meta[idb];

            uint64_t vsA = A.v_startPage, vsB = B.v_startPage;
            uint64_t vlA = (A.v_order < 64) ? (1ull << A.v_order) : UINT64_MAX;
            uint64_t vlB = (B.v_order < 64) ? (1ull << B.v_order) : UINT64_MAX;
            if (math_utils::intersects(vsA, vlA, vsB, vlB)) v_pairs.emplace_back(ida, idb);

            uint64_t isA = A.i_startPage, isB = B.i_startPage;
            uint64_t ilA = (A.i_order < 64) ? (1ull << A.i_order) : UINT64_MAX;
            uint64_t ilB = (B.i_order < 64) ? (1ull << B.i_order) : UINT64_MAX;
            if (math_utils::intersects(isA, ilA, isB, ilB)) i_pairs.emplace_back(ida, idb);
        }
    }
    //=================РАСЧЁТ ПЕРЕСЕЧЕНИЙ=================


    const uint32_t ST_MASK_BITS = 4;
    const uint32_t ST_FREE    = 0u;
    const uint32_t ST_ALLOC   = 1u;
    const uint32_t ST_MERGED  = 2u;
    const uint32_t ST_MERGING = 3u;
    const uint32_t ST_READY = 4u;
    const uint32_t ST_CONCEDED = 5u;
    const uint32_t ST_MASK   = (1u << ST_MASK_BITS) - 1u;

    uint32_t count_vb_free = 0, vb_free_pages = 0;
    uint32_t count_vb_alloc = 0, vb_alloc_pages = 0;
    uint32_t count_vb_merged = 0, vb_merged_pages = 0;
    uint32_t count_vb_merging = 0, vb_merging_pages = 0;
    uint32_t count_vb_ready = 0, vb_ready_pages = 0;
    uint32_t count_vb_conceded = 0, vb_conceded_pages = 0;
    for (uint32_t i = 0; i < count_vb_pages_; i++) {
        uint32_t kind = vb_states[i] & ST_MASK;
        uint32_t order = vb_states[i] >> ST_MASK_BITS;
        uint32_t count_pages = 1u << order;
        if (kind == ST_FREE) {count_vb_free++; vb_free_pages += count_pages; }
        if (kind == ST_ALLOC) {count_vb_alloc++; vb_alloc_pages += count_pages; }
        if (kind == ST_MERGED) {count_vb_merged++; vb_merged_pages += count_pages; }
        if (kind == ST_MERGING) {count_vb_merging++; vb_merging_pages += count_pages; }
        if (kind == ST_READY) {count_vb_ready++; vb_ready_pages += count_pages; }
        if (kind == ST_CONCEDED) {count_vb_conceded++; vb_conceded_pages += count_pages; }
    }

    uint32_t count_ib_free = 0, ib_free_pages = 0;
    uint32_t count_ib_alloc = 0, ib_alloc_pages = 0;
    uint32_t count_ib_merged = 0, ib_merged_pages = 0;
    uint32_t count_ib_merging = 0, ib_merging_pages = 0;
    uint32_t count_ib_ready = 0, ib_ready_pages = 0;
    uint32_t count_ib_conceded = 0, ib_conceded_pages = 0;
    for (uint32_t i = 0; i < count_ib_pages_; i++) {
        uint32_t kind = ib_states[i] & ST_MASK;
        uint32_t order = ib_states[i] >> ST_MASK_BITS;
        uint32_t count_pages = 1u << order;
        if (kind == ST_FREE) {count_ib_free++; ib_free_pages += count_pages; }
        if (kind == ST_ALLOC) {count_ib_alloc++; ib_alloc_pages += count_pages; }
        if (kind == ST_MERGED) {count_ib_merged++; ib_merged_pages += count_pages; }
        if (kind == ST_MERGING) {count_ib_merging++; ib_merging_pages += count_pages; }
        if (kind == ST_READY) {count_ib_ready++; ib_ready_pages += count_pages; }
        if (kind == ST_CONCEDED) {count_ib_conceded++; ib_conceded_pages += count_pages; }
    }

    std::cout << "---INTERSECTIONS---" << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "Count vb intersections: " << v_pairs.size() << std::endl;
    std::cout << std::endl;
    std::cout << "==Index buffer==" << std::endl;
    std::cout << "Count ib intersections: " << i_pairs.size() << std::endl;
    std::cout << std::endl;

    std::cout << "---DATA FROM META---"      << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "ST_ALLOC:      count chunks = " << count_vb_alloc_chunks_from_meta << " count pages = " << count_alloc_vb_pages_from_meta << std::endl;
    std::cout << std::endl;
    std::cout << "==Index buffer==" << std::endl;
    std::cout << "ST_ALLOC:      count chunks = " << count_ib_alloc_chunks_from_meta << " count pages = " << count_alloc_ib_pages_from_meta << std::endl;
    std::cout << std::endl;

    std::cout << "---DATA FROM STATES BUFFER---" << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_vb_free     << "  count pages = " << vb_free_pages << std::endl;
    std::cout << "ST_ALLOC:     count states = " << count_vb_alloc    << "  count pages = " << vb_alloc_pages << std::endl;
    std::cout << "ST_MERGED:    count states = " << count_vb_merged   << "  count pages = " << vb_merged_pages << std::endl;
    std::cout << "ST_MERGING:   count states = " << count_vb_merging  << "  count pages = " << vb_merging_pages << std::endl;
    std::cout << "ST_READY:     count states = " << count_vb_ready    << "  count pages = " << vb_ready_pages << std::endl;
    std::cout << "ST_CONCEDED:  count states = " << count_vb_conceded << "  count pages = " << vb_conceded_pages << std::endl;
    std::cout << "SUM (free + alloc): count states = " << count_vb_free + count_vb_alloc << "  count pages = " << vb_free_pages + vb_alloc_pages << std::endl;
    std::cout << "REAL_COUNT_PAGES: " << count_vb_pages_ << std::endl;
    std::cout << "LIMBO: " << count_vb_pages_ - (vb_free_pages + vb_alloc_pages) << std::endl;
    std::cout << std::endl;
    std::cout << "==Index buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_ib_free     << "  count pages = " << ib_free_pages << std::endl;
    std::cout << "ST_ALLOC:     count states = " << count_ib_alloc    << "  count pages = " << ib_alloc_pages << std::endl;
    std::cout << "ST_MERGED:    count states = " << count_ib_merged   << "  count pages = " << ib_merged_pages << std::endl;
    std::cout << "ST_MERGING:   count states = " << count_ib_merging  << "  count pages = " << ib_merging_pages << std::endl;
    std::cout << "ST_READY:     count states = " << count_ib_ready    << "  count pages = " << ib_ready_pages << std::endl;
    std::cout << "ST_CONCEDED:  count states = " << count_ib_conceded << "  count pages = " << ib_conceded_pages << std::endl;
    std::cout << "SUM (free + alloc): count states = " << count_ib_free + count_ib_alloc << "  count pages = " << ib_free_pages + ib_alloc_pages << std::endl;
    std::cout << "REAL_COUNT_PAGES: " << count_ib_pages_ << std::endl;
    std::cout << "LIMBO: " << count_ib_pages_ - (ib_free_pages + ib_alloc_pages) << std::endl;
    std::cout << std::endl;
}

void VoxelGridGPU::load_alloc_stacks() {
    std::ifstream vb_in(executable_dir() / "vb_alloc_stack_with_bug.txt");
    std::vector<uint32_t> vb_stack;
    uint32_t value;
    while(vb_in >> value) {
        vb_stack.push_back(value);
    }
    vb_in.close();

    std::ifstream ib_in(executable_dir() / "ib_alloc_stack_with_bug.txt");
    std::vector<uint32_t> ib_stack;
    while(ib_in >> value) {
        ib_stack.push_back(value);
    }
    ib_in.close();

    vb_alloc_stack_ = SSBO(sizeof(uint32_t) * vb_stack.size(), GL_DYNAMIC_DRAW, vb_stack.data());
    ib_alloc_stack_ = SSBO(sizeof(uint32_t) * ib_stack.size(), GL_DYNAMIC_DRAW, ib_stack.data());
}

void VoxelGridGPU::init_programs(ShaderManager& shader_manager) {
    prog_clear_chunks_ = ComputeProgram(&shader_manager.clear_chunks_cs);
    prog_set_chunks_ = ComputeProgram(&shader_manager.set_chunks_cs);;

    prog_world_init_ = ComputeProgram(&shader_manager.world_init_cs);
    prog_apply_writes_ = ComputeProgram(&shader_manager.apply_writes_to_world_cs);
    prog_mesh_counters_reset_ = ComputeProgram(&shader_manager.mesh_counters_reset_cs);
    prog_mesh_reset_ = ComputeProgram(&shader_manager.mesh_reset_cs);
    prog_mesh_count_ = ComputeProgram(&shader_manager.mesh_count_cs);
    prog_mesh_alloc_ = ComputeProgram(&shader_manager.mesh_alloc_cs);
    prog_mesh_emit_ = ComputeProgram(&shader_manager.mesh_emit_cs);
    prog_mesh_finalize_ = ComputeProgram(&shader_manager.mesh_finalize_cs);
    prog_cmdcount_reset_ = ComputeProgram(&shader_manager.cmdcount_reset_cs);
    prog_build_indirect_ = ComputeProgram(&shader_manager.build_indirect_cmds_cs);
    prog_reset_dirty_count_ = ComputeProgram(&shader_manager.reset_dirty_count_cs);
    prog_bucket_reset_ = ComputeProgram(&shader_manager.bucket_reset_cs);
    prog_bucket_build_ = ComputeProgram(&shader_manager.bucket_build_cs);
    prog_evict_lowprio_ = ComputeProgram(&shader_manager.evict_lowprio_cs);
    prog_stream_select_chunks_ = ComputeProgram(&shader_manager.stream_select_chunks_cs);
    prog_stream_generate_terrain_ = ComputeProgram(&shader_manager.stream_generate_terrain_cs);
    prog_mark_all_user_chunks_as_dirty_ = ComputeProgram(&shader_manager.mark_all_user_chunks_as_dirty_cs);
    prog_mesh_pool_clear_ = ComputeProgram(&shader_manager.mesh_pool_clear_cs);
    prog_mesh_pool_seed_ = ComputeProgram(&shader_manager.mesh_pool_seed_cs);
    prog_reset_load_list_counter_ = ComputeProgram(&shader_manager.reset_load_list_counter_cs);
    prog_verify_mesh_allocation_ = ComputeProgram(&shader_manager.verify_mesh_allocation_cs);

    prog_vf_voxel_mesh_diffusion_spec_ = VfProgram(&shader_manager.voxel_mesh_vs, &shader_manager.voxel_mesh_fs);
}

void VoxelGridGPU::set_frame_counters(uint32_t write_count, uint32_t dirty_count, uint32_t cmd_count, uint32_t free_count) {
    if (write_count != DONT_CHANGE)
        frame_counters_.update_subdata(0, &write_count, sizeof(uint32_t));
    
    if (dirty_count != DONT_CHANGE)
        frame_counters_.update_subdata(4, &dirty_count, sizeof(uint32_t));
    
    if (cmd_count != DONT_CHANGE)
        frame_counters_.update_subdata(8, &cmd_count, sizeof(uint32_t));
    
    if (free_count != DONT_CHANGE)
        frame_counters_.update_subdata(12, &free_count, sizeof(uint32_t));
}

uint32_t VoxelGridGPU::read_dirty_count_cpu() {
    uint32_t dirtyCount = 0;
    frame_counters_.read_subdata(4, &dirtyCount, sizeof(uint32_t));
    return dirtyCount;
}

void VoxelGridGPU::world_init_gpu() {
    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);
    free_list_.bind_base(4);
    frame_counters_.bind_base(5);
    chunk_meta_.bind_base(6);
    equeued_.bind_base(7);
    count_free_pages_.bind_base(8);

    prog_world_init_.use();
    glUniform1ui(glGetUniformLocation(prog_world_init_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_world_init_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_world_init_.id, "u_count_vb_pages"), count_vb_pages_);
    glUniform1ui(glGetUniformLocation(prog_world_init_.id, "u_count_ib_pages"), count_ib_pages_);

    uint32_t maxItems = std::max(chunk_hash_table_size, count_active_chunks);
    uint32_t groups_x = math_utils::div_up_u32(maxItems, 256u);

    prog_world_init_.dispatch_compute(groups_x, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::clear_chunks(std::vector<uint32_t>& chunk_ids, const VoxelDataGPU& init_voxel_prifab) {
    if (chunk_ids.size() == 0) return;

    size_t need = sizeof(uint32_t) * chunk_ids.size();
    if (chunk_indices_to_clear_cap_bytes_ < need) {
        chunk_indices_to_clear_ = SSBO(need, GL_DYNAMIC_DRAW, nullptr);
        chunk_indices_to_clear_cap_bytes_ = need;
    }

    chunk_indices_to_clear_.update_discard(chunk_ids.data(), need);
    voxel_prifab_.update_discard(&init_voxel_prifab, sizeof(VoxelDataGPU));

    uint32_t chunk_voxel_count = chunk_size.x * chunk_size.y * chunk_size.z;
    uint32_t groups_x = math_utils::div_up_u32(chunk_voxel_count, 256u);
    uint32_t groups_y = chunk_ids.size();
    uint32_t groups_z = 1;

    voxels_.bind_base(0);
    chunk_indices_to_clear_.bind_base(1);
    voxel_prifab_.bind_base(2);

    prog_clear_chunks_.use();
    glUniform1ui(glGetUniformLocation(prog_clear_chunks_.id, "u_count_chunks_to_clear"), chunk_ids.size());
    glUniform1ui(glGetUniformLocation(prog_clear_chunks_.id, "u_chunk_size"), chunk_voxel_count);
    prog_clear_chunks_.dispatch_compute(groups_x, groups_y, groups_z);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::init_mesh_pool() {
    // Pass 1: mesh_pool_clear
    vb_heads_.bind_base(18);
    vb_next_.bind_base(19);
    vb_state_.bind_base(20);

    ib_heads_.bind_base(21);
    ib_next_.bind_base(22);
    ib_state_.bind_base(23);

    chunk_mesh_alloc_.bind_base(24);

    prog_mesh_pool_clear_.use();

    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_pages"), count_vb_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_pages"), count_ib_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_heads_count"), vb_order_ + 1);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_heads_count"), ib_order_ + 1);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_index_bits"),  vb_index_bits_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_index_bits"),  ib_index_bits_);

    uint32_t groups_x = math_utils::div_up_u32(std::max(std::max(count_vb_pages_, count_ib_pages_), count_active_chunks), 256u);
    prog_mesh_pool_clear_.dispatch_compute(groups_x, 1, 1);
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 2: mesh_pool_seed
    vb_heads_.bind_base(18);
    vb_next_.bind_base(19);
    vb_state_.bind_base(20);

    ib_heads_.bind_base(21);
    ib_next_.bind_base(22);
    ib_state_.bind_base(23);

    prog_mesh_pool_seed_.use();

    glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_vb_max_order"), vb_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_ib_max_order"), ib_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_vb_index_bits"),  vb_index_bits_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_ib_index_bits"),  ib_index_bits_);

    prog_mesh_pool_seed_.dispatch_compute(1, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::init_active_chunks(glm::ivec3 chunk_size, uint32_t count_active_chunks, const VoxelDataGPU& init_voxel_prifab) {
    uint32_t chunk_voxel_count = chunk_size.x * chunk_size.y * chunk_size.z;
    voxels_ = SSBO(sizeof(VoxelDataGPU) * chunk_voxel_count * count_active_chunks, GL_DYNAMIC_DRAW);

    std::vector<uint32_t> chunk_ids(count_active_chunks);
    for (uint32_t i = 0; i < count_active_chunks; i++) chunk_ids[i] = i;
    clear_chunks(chunk_ids, init_voxel_prifab);
}

void VoxelGridGPU::init_chunks_hash_table() {
    chunk_hash_keys_ = SSBO(sizeof(glm::uvec2) * chunk_hash_table_size, GL_DYNAMIC_DRAW);
    chunk_hash_vals_ = SSBO(sizeof(uint32_t) * chunk_hash_table_size, GL_DYNAMIC_DRAW);
}

void VoxelGridGPU::set_chunks(const std::vector<uint32_t>& chunk_ids, const std::vector<glm::ivec3>& chunk_coords, bool set_with_replace) {
    std::vector<uint64_t> coord_keys(chunk_coords.size());
    for (uint32_t i = 0; i < chunk_coords.size(); i++) 
        coord_keys[i] = math_utils::pack_key(chunk_coords[i].x, chunk_coords[i].y, chunk_coords[i].z);
    
    set_chunks(chunk_ids, coord_keys, set_with_replace);
}


void VoxelGridGPU::set_chunks(const std::vector<uint32_t>& chunk_ids,
                              const std::vector<uint64_t>& coord_keys,
                              bool set_with_replace) {
    assert(chunk_ids.size() == coord_keys.size());

    std::vector<glm::uvec2> keys2(coord_keys.size());
    for (size_t i = 0; i < coord_keys.size(); ++i) keys2[i] = math_utils::split_u64(coord_keys[i]);

    // ensure buffers (uint32 ids + uvec2 keys)
    size_t need_ids = sizeof(uint32_t) * chunk_ids.size();
    if (need_ids > chunk_indices_to_set_cap_bytes_) {
        chunk_indices_to_set_ = SSBO(need_ids, GL_DYNAMIC_DRAW);
        chunk_indices_to_set_cap_bytes_ = need_ids;
    }

    size_t need_keys = sizeof(glm::uvec2) * keys2.size();
    if (need_keys > coord_keys_to_set_cap_bytes_) {
        coord_keys_to_set_ = SSBO(need_keys, GL_DYNAMIC_DRAW);
        coord_keys_to_set_cap_bytes_ = need_keys;
    }

    chunk_indices_to_set_.update_discard(chunk_ids.data(), need_ids);
    coord_keys_to_set_.update_discard(keys2.data(), need_keys);

    uint32_t groups_x = math_utils::div_up_u32((uint32_t)chunk_ids.size(), 256u);

    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);
    chunk_indices_to_set_.bind_base(2);
    coord_keys_to_set_.bind_base(3);
    
    prog_set_chunks_.use();
    glUniform1ui(glGetUniformLocation(prog_set_chunks_.id, "u_count_chunks_to_set"), (uint32_t)chunk_ids.size());
    glUniform1ui(glGetUniformLocation(prog_set_chunks_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_set_chunks_.id, "u_max_count_probing"), max_count_probing);
    glUniform1ui(glGetUniformLocation(prog_set_chunks_.id, "u_set_with_replace"), set_with_replace ? 1u : 0u);
    prog_set_chunks_.dispatch_compute(groups_x, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::ensure_set_chunk_buffers(const std::vector<uint32_t>& chunk_ids, const std::vector<uint64_t>& coord_keys) {
    size_t need_ids_cap = sizeof(uint32_t) * chunk_ids.size();
    if (need_ids_cap > chunk_indices_to_set_cap_bytes_) {
        chunk_indices_to_set_ = SSBO(need_ids_cap, GL_DYNAMIC_DRAW);
        chunk_indices_to_set_cap_bytes_ = need_ids_cap;
    }

    size_t need_coord_keys_cap = sizeof(uint64_t) * coord_keys.size();
    if (need_coord_keys_cap > coord_keys_to_set_cap_bytes_) {
        coord_keys_to_set_ = SSBO(need_coord_keys_cap, GL_DYNAMIC_DRAW);
        coord_keys_to_set_cap_bytes_ = need_coord_keys_cap;
    }
}

void VoxelGridGPU::ensure_free_chunks_gpu(const glm::vec3& camPos) {
    // 1) reset heads
    bucket_heads_.bind_base(16);

    prog_bucket_reset_.use();
    glUniform1ui(glGetUniformLocation(prog_bucket_reset_.id, "u_bucket_count"), this->count_evict_buckets);

    uint32_t gx = math_utils::div_up_u32(this->count_evict_buckets, 256u);
    prog_bucket_reset_.dispatch_compute(gx, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 2) build bucket lists
    chunk_meta_.bind_base(6);
    bucket_heads_.bind_base(16);
    bucket_next_.bind_base(17);

    prog_bucket_build_.use();
    glUniform1ui(glGetUniformLocation(prog_bucket_build_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_bucket_build_.id, "u_bucket_count"), this->count_evict_buckets);
    glUniform3f(glGetUniformLocation(prog_bucket_build_.id, "u_cam_pos"), camPos.x, camPos.y, camPos.z);
    glUniform3i(glGetUniformLocation(prog_bucket_build_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_bucket_build_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniform1ui(glGetUniformLocation(prog_bucket_build_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i (glGetUniformLocation(prog_bucket_build_.id, "u_pack_offset"), math_utils::OFFSET);

    // scale: dist2 * scale -> bucket
    // step (м) — размер “полосы” расстояний на бакет
    float scale = 1.0f / (this->bucket_step * this->bucket_step);
    glUniform1f(glGetUniformLocation(prog_bucket_build_.id, "u_bucket_scale"), scale);

    gx = math_utils::div_up_u32(count_active_chunks, 256u);
    prog_bucket_build_.dispatch_compute(gx, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 3) evict if needed (shader сам проверит freeCount)
    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);
    free_list_.bind_base(4);
    frame_counters_.bind_base(5);
    chunk_meta_.bind_base(6);
    equeued_.bind_base(7);

    bucket_heads_.bind_base(16);
    bucket_next_.bind_base(17);

    prog_evict_lowprio_.use();
    glUniform1ui(glGetUniformLocation(prog_evict_lowprio_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_evict_lowprio_.id, "u_bucket_count"), count_evict_buckets);
    glUniform1ui(glGetUniformLocation(prog_evict_lowprio_.id, "u_min_free"), min_free_chunks);
    glUniform1ui(glGetUniformLocation(prog_evict_lowprio_.id, "u_max_evict"), max_evict_chunks);

    prog_evict_lowprio_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::ensure_voxel_write_list(size_t count) {
    size_t need = sizeof(VoxelWriteGPU) * count;
    if (need == 0) return;

    if (voxel_write_list_cap_bytes_ < need) {
        voxel_write_list_ = SSBO(need, GL_DYNAMIC_DRAW, nullptr);
        voxel_write_list_cap_bytes_ = need;
    }
}

void VoxelGridGPU::apply_writes_to_world_gpu(uint32_t write_count) {
    assert(voxel_write_list_cap_bytes_ / sizeof(VoxelWriteGPU) >= write_count);
    if (write_count == 0) return;

    set_frame_counters(write_count);

    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);
    voxel_write_list_.bind_base(2);
    voxels_.bind_base(3);
    free_list_.bind_base(4);
    frame_counters_.bind_base(5);
    chunk_meta_.bind_base(6);
    equeued_.bind_base(7);
    dirty_list_.bind_base(8);

    prog_apply_writes_.use();

    glUniform1ui(glGetUniformLocation(prog_apply_writes_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_apply_writes_.id, "u_chunk_dim"),
                chunk_size.x, chunk_size.y, chunk_size.z);
    uint32_t vox_per_chunk = chunk_size.x * chunk_size.y * chunk_size.z;
    glUniform1ui(glGetUniformLocation(prog_apply_writes_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform1ui(glGetUniformLocation(prog_apply_writes_.id, "u_set_dirty_flag_bits"), 1u);

    uint32_t groups_x = math_utils::div_up_u32(write_count, 256u);
    prog_apply_writes_.dispatch_compute(groups_x, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::apply_writes_to_world_from_cpu(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels) {
    assert(positions.size() == voxels.size());

    std::vector<VoxelWriteGPU> voxel_writes;
    voxel_writes.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++) {
        voxel_writes[i].world_voxel = glm::ivec4(positions[i].x, positions[i].y, positions[i].z, 1);
        voxel_writes[i].voxel_data = voxels[i];
    }

    ensure_voxel_write_list(voxel_writes.size());
    voxel_write_list_.update_discard(voxel_writes.data(), sizeof(VoxelWriteGPU) * voxel_writes.size());

    apply_writes_to_world_gpu(voxel_writes.size());
}

void VoxelGridGPU::apply_writes_to_world_gpu_with_evict(uint32_t write_count, const glm::vec3& cam_pos) {
    ensure_free_chunks_gpu(cam_pos);
    apply_writes_to_world_gpu(write_count);
}

void VoxelGridGPU::apply_writes_to_world_from_cpu_with_evict(
    const std::vector<glm::ivec3>& positions, 
    const std::vector<VoxelDataGPU>& voxels,
    const glm::vec3& cam_pos) {
    ensure_free_chunks_gpu(cam_pos);
    apply_writes_to_world_from_cpu(positions, voxels);
}

void VoxelGridGPU::mark_chunk_to_generate(const glm::vec3& cam_world_pos, int radius_chunks)
{
    // камера в локальные координаты грида (важно, если VoxelGridGPU трансформируется)
    glm::mat4 invM = glm::inverse(get_model_matrix());
    glm::vec3 cam_local = glm::vec3(invM * glm::vec4(cam_world_pos, 1.0f));

    // reset loadCount = 0
    uint32_t zero2[2] = {0, 0};
    stream_counters_.update_subdata(0, zero2, sizeof(zero2));

    // ---- pass 1: select/create ----
    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);
    free_list_.bind_base(4);
    frame_counters_.bind_base(5);
    chunk_meta_.bind_base(6);
    equeued_.bind_base(7);

    stream_counters_.bind_base(16);
    load_list_.bind_base(17);

    prog_stream_select_chunks_.use();
    glUniform1ui(glGetUniformLocation(prog_stream_select_chunks_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_stream_select_chunks_.id, "u_max_load_entries"), count_active_chunks);
    glUniform3i(glGetUniformLocation(prog_stream_select_chunks_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_stream_select_chunks_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform3f(glGetUniformLocation(prog_stream_select_chunks_.id, "u_cam_pos_local"), cam_local.x, cam_local.y, cam_local.z);
    glUniform1i(glGetUniformLocation(prog_stream_select_chunks_.id, "u_radius_chunks"), radius_chunks);
    glUniform1ui(glGetUniformLocation(prog_stream_select_chunks_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i(glGetUniformLocation(prog_stream_select_chunks_.id, "u_pack_offset"), math_utils::OFFSET);

    const uint32_t side = (uint32_t)(2 * radius_chunks + 1);
    const uint32_t gx = math_utils::div_up_u32(side, 8u);
    const uint32_t gy = math_utils::div_up_u32(side, 8u);
    const uint32_t gz = math_utils::div_up_u32(side, 8u);

    prog_stream_select_chunks_.dispatch_compute(gx, gy, gz);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::generate_terrain(uint32_t seed, uint32_t load_count) {
    if (load_count > count_active_chunks) load_count = count_active_chunks;

    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);

    stream_counters_.bind_base(16);
    load_list_.bind_base(17);

    voxels_.bind_base(3);
    chunk_meta_.bind_base(6);
    equeued_.bind_base(7);
    dirty_list_.bind_base(8);
    frame_counters_.bind_base(5);

    stream_generate_debug_counters_.bind_base(25);

    prog_stream_generate_terrain_.use();
    glUniform3i(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    uint32_t vox_per_chunk = (uint32_t)(chunk_size.x * chunk_size.y * chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform3f(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_set_dirty_flag_bits"), 1u);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_seed"), seed);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_hash_table_size"), chunk_hash_table_size);

    uint32_t groups_x = math_utils::div_up_u32(vox_per_chunk, 256u);
    prog_stream_generate_terrain_.dispatch_compute(groups_x, load_count, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::reset_load_list_counter() {
    stream_counters_.bind_base(0);
    prog_reset_load_list_counter_.use();
    prog_reset_load_list_counter_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::stream_chunks_sphere(const glm::vec3& cam_world_pos, int radius_chunks, uint32_t seed) {
    // double t0 = math_utils::ms_now();

    mark_chunk_to_generate(cam_world_pos, radius_chunks);
    // glFinish();
    
    // double t1 = math_utils::ms_now();

    uint32_t load_count = 0;
    stream_counters_.read_subdata(0, &load_count, sizeof(uint32_t));
    if (load_count == 0) return;

    // double t2 = math_utils::ms_now();

    // world generation
    generate_terrain(seed, load_count);
    // glFinish();

    // double t3 = math_utils::ms_now();

    reset_load_list_counter();
    // glFinish();

    // double t4 = math_utils::ms_now();

    // std::cout << "mark_chunk_to_generate(): " << t1 - t0 << std::endl;
    // std::cout << "readback: " << t2 - t1 << std::endl;
    // std::cout << "generate_terrain(): " << t3 - t2 << std::endl;
    // std::cout << "reset_load_list_counter(): " << t4 - t3 << std::endl;
    // std::cout << "all: " << t4 - t0 << std::endl;
    // mark_all_used_chunks_as_dirty();
}

void VoxelGridGPU::build_mesh_from_dirty(uint32_t pack_bits, int pack_offset) {
    uint32_t dirtyCount = read_dirty_count_cpu();
    if (dirtyCount == 0) return;

    uint32_t vox_per_chunk = (uint32_t)(chunk_size.x * chunk_size.y * chunk_size.z);
    uint32_t groups_vox = math_utils::div_up_u32(vox_per_chunk, 256u); // для 4096 будет 16
    uint32_t groups_dirty = math_utils::div_up_u32(dirtyCount, 256u);

    // std::cout << "DIRTY COUT: " << dirtyCount << std::endl;

    // ---- Pass: reset global mesh counters (GPU) ----
    mesh_counters_.bind_base(0);
    prog_mesh_counters_reset_.use();
    prog_mesh_counters_reset_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Pass: mesh_reset ----
    frame_counters_.bind_base(0);
    dirty_list_.bind_base(1);
    dirty_quad_count_.bind_base(2);
    emit_counter_.bind_base(3);

    prog_mesh_reset_.use();
    prog_mesh_reset_.dispatch_compute(groups_dirty, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Pass: mesh_count ----
    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);
    voxels_.bind_base(2);
    frame_counters_.bind_base(3);
    dirty_list_.bind_base(4);
    dirty_quad_count_.bind_base(5);
    chunk_meta_.bind_base(6);

    prog_mesh_count_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_count_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_mesh_count_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_mesh_count_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform1ui(glGetUniformLocation(prog_mesh_count_.id, "u_pack_bits"), pack_bits);
    glUniform1i(glGetUniformLocation(prog_mesh_count_.id, "u_pack_offset"), pack_offset);

    prog_mesh_count_.dispatch_compute(groups_vox, dirtyCount, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Pass: mesh_alloc ----
    frame_counters_.bind_base(0);
    dirty_list_.bind_base(1);
    dirty_quad_count_.bind_base(2);

    chunk_meta_.bind_base(3);

    chunk_mesh_alloc_local_.bind_base(4);
    chunk_mesh_alloc_.bind_base(5);

    vb_heads_.bind_base(6);
    vb_next_.bind_base(7);
    vb_state_.bind_base(8);

    ib_heads_.bind_base(9);
    ib_next_.bind_base(10);
    ib_state_.bind_base(11);

    debug_counters_.bind_base(12);
    count_free_pages_.bind_base(13);
    vb_alloc_stack_.bind_base(14);
    ib_alloc_stack_.bind_base(15);

    prog_mesh_alloc_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_max_vertices"), max_mesh_vertices_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_max_indices"),  max_mesh_indices_);

    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_vb_pages"),  count_vb_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_ib_pages"),  count_ib_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_vb_page_verts"), vb_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_ib_page_inds"),  ib_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_vb_max_order"),  vb_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_ib_max_order"),  ib_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_vb_index_bits"),  vb_index_bits_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_ib_index_bits"),  ib_index_bits_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_min_free_pages"),  min_free_pages);

    prog_mesh_alloc_.dispatch_compute(groups_dirty, 1, 1);
    // prog_mesh_alloc_.dispatch_compute(10, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // 2048

    // uint32_t free_pages[2] = {0};
    // count_free_pages_.read_subdata(0, free_pages, sizeof(uint32_t) * 2);

    // auto& print_diff = [&](auto& diff, auto& meta_alloc, const std::string& name) {
    //     if (diff.different.size() > 0) {
    //         std::cout << "==========" << name << " DIFFERENCE==========" << std::endl;
    //         for (auto& p : diff.different) {
    //             if (meta_alloc.find(p.first) != meta_alloc.end()) 
    //                 std::cout << "META:    " << "start = " << p.first << "    order(META) = " << p.second.first << std::endl;
    //             else
    //                 std::cout << "STATE:    " << "start = " << p.first << "    order = " << p.second.second << std::endl;
    //         }
    //         std::cout << std::endl;
    //     }
    // };

    // std::unordered_map<uint32_t, uint32_t> meta_alloc_vb;
    // std::unordered_map<uint32_t, uint32_t> meta_alloc_ib;
    // std::unordered_map<uint32_t, uint32_t> states_alloc_vb; 
    // std::unordered_map<uint32_t, uint32_t> states_alloc_ib;
    // read_states_data(meta_alloc_vb, meta_alloc_ib, states_alloc_vb, states_alloc_ib);

    // auto diff_vb = math_utils::diff_maps(meta_alloc_vb, states_alloc_vb);
    // auto diff_ib = math_utils::diff_maps(meta_alloc_ib, states_alloc_ib);

    
    // std::cout << "=======ALLOCATION PASS=======" << std::endl;

    // if (diff_vb.different.size() > 0 || diff_ib.different.size() > 0) {
    //     print_diff(diff_vb, meta_alloc_vb, "VB");
    //     print_diff(diff_ib, meta_alloc_ib, "IB");
    // } else {
    //     std::cout << "--------" << std::endl;
    // }

    // ---- Pass: verify_mesh_allocation ----
    chunk_mesh_alloc_local_.bind_base(0);
    chunk_mesh_alloc_.bind_base(1);
    count_free_pages_.bind_base(2);
    dirty_list_.bind_base(3);
    frame_counters_.bind_base(4);
    
    vb_heads_.bind_base(5);
    vb_next_.bind_base(6);
    vb_state_.bind_base(7);
    
    ib_heads_.bind_base(8);
    ib_next_.bind_base(9);
    ib_state_.bind_base(10);
    vb_alloc_stack_.bind_base(11);
    ib_alloc_stack_.bind_base(12);
    debug_counters_.bind_base(13);

    prog_verify_mesh_allocation_.use();
    glUniform1ui(glGetUniformLocation(prog_verify_mesh_allocation_.id, "u_min_free_pages"),  min_free_pages);
    glUniform1ui(glGetUniformLocation(prog_verify_mesh_allocation_.id, "u_vb_max_order"),  vb_order_);
    glUniform1ui(glGetUniformLocation(prog_verify_mesh_allocation_.id, "u_ib_max_order"),  ib_order_);

    prog_verify_mesh_allocation_.dispatch_compute(groups_dirty, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // count_free_pages_.read_subdata(0, free_pages, sizeof(uint32_t) * 2);
    // std::cout << "VB FREE PAGES[alloc+verify]: " << free_pages[0] << "    ";
    // std::cout << "IB FREE PAGES[alloc+verify]: " << free_pages[1] << std::endl;;

    // read_states_data(meta_alloc_vb, meta_alloc_ib, states_alloc_vb, states_alloc_ib);

    // diff_vb = math_utils::diff_maps(meta_alloc_vb, states_alloc_vb);
    // diff_ib = math_utils::diff_maps(meta_alloc_ib, states_alloc_ib);

    
    // std::cout << "=======VERIFICATION PASS=======" << std::endl;

    // if (diff_vb.different.size() > 0 || diff_ib.different.size() > 0) {
    //     print_diff(diff_vb, meta_alloc_vb, "VB");
    //     print_diff(diff_ib, meta_alloc_ib, "IB");
    // } else {
    //     std::cout << "--------" << std::endl;
    // }

    std::cout << std::endl;

    // ---- Pass: mesh_emit ----
    chunk_hash_keys_.bind_base(0);
    chunk_hash_vals_.bind_base(1);

    voxels_.bind_base(2);
    frame_counters_.bind_base(3);
    dirty_list_.bind_base(4);
    emit_counter_.bind_base(5);
    chunk_mesh_alloc_.bind_base(6);

    chunk_meta_.bind_base(7);
    count_free_pages_.bind_base(8);

    global_vertex_buffer_.bind_base(9);
    global_index_buffer_.bind_base(10);

    prog_mesh_emit_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_mesh_emit_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform3f(glGetUniformLocation(prog_mesh_emit_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_pack_bits"), pack_bits);
    glUniform1i(glGetUniformLocation(prog_mesh_emit_.id, "u_pack_offset"), pack_offset);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_vb_page_verts"), vb_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_ib_page_inds"), ib_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_min_free_pages"),  min_free_pages);

    prog_mesh_emit_.dispatch_compute(groups_vox, dirtyCount, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Pass: mesh_finalize ----
    frame_counters_.bind_base(0);
    dirty_list_.bind_base(1);
    equeued_.bind_base(2);
    chunk_meta_.bind_base(3);
    chunk_mesh_alloc_.bind_base(4);
    failed_dirty_list_.bind_base(5);

    prog_mesh_finalize_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_finalize_.id, "u_dirty_flag_bits"), 1u);

    prog_mesh_finalize_.dispatch_compute(groups_dirty, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ---- Pass: rest_dirty_count ----
    frame_counters_.bind_base(0);
    prog_reset_dirty_count_.use();
    prog_reset_dirty_count_.dispatch_compute(1,1,1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::build_indirect_draw_commands_frustum(const glm::mat4& viewProj,
                                                        uint32_t pack_bits,
                                                        int pack_offset) {
    // 1) reset cmdCount (counters.z = 0)
    frame_counters_.bind_base(5);
    prog_cmdcount_reset_.use();
    prog_cmdcount_reset_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 2) compute planes on CPU and upload as uniforms
    auto planes = math_utils::extract_frustum_planes(viewProj);

    frame_counters_.bind_base(5);
    chunk_meta_.bind_base(6);
    chunk_mesh_alloc_.bind_base(24);
    indirect_cmds_.bind_base(15);

    prog_build_indirect_.use();

    glUniform1ui(glGetUniformLocation(prog_build_indirect_.id, "u_max_chunks"), count_active_chunks);
    glUniform3i(glGetUniformLocation(prog_build_indirect_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_build_indirect_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniform1ui(glGetUniformLocation(prog_build_indirect_.id, "u_pack_bits"), pack_bits);
    glUniform1i(glGetUniformLocation(prog_build_indirect_.id, "u_pack_offset"), pack_offset);

    glUniform1ui(glGetUniformLocation(prog_build_indirect_.id, "u_vb_page_verts"), vb_page_size_);
    glUniform1ui(glGetUniformLocation(prog_build_indirect_.id, "u_ib_page_inds"), ib_page_size_);

    // u_frustum_planes[6]
    GLint loc = glGetUniformLocation(prog_build_indirect_.id, "u_frustum_planes");
    glUniform4fv(loc, 6, &planes[0].x);

    uint32_t groups_x = math_utils::div_up_u32(count_active_chunks, 256u);
    prog_build_indirect_.dispatch_compute(groups_x, 1, 1);

    // важно: команды и буферы будут читаться в draw
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
}

void VoxelGridGPU::draw_indirect(const GLuint vao, const glm::mat4& world, const glm::mat4& proj_view, const glm::vec3& cam_pos) {
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
            GL_ELEMENT_ARRAY_BARRIER_BIT |
            GL_COMMAND_BARRIER_BIT);

    glBindVertexArray(vao);

    // command buffer
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_cmds_.id());

    // индексы
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_index_buffer_.id());

    const GLsizei stride = (GLsizei)sizeof(DrawElementsIndirectCommand);
    const GLsizei maxDraws = (GLsizei)count_active_chunks;

    auto attash_shader_program = [&](){
        prog_vf_voxel_mesh_diffusion_spec_.use();
        glUniformMatrix4fv(glGetUniformLocation(prog_vf_voxel_mesh_diffusion_spec_.id,"uWorld"), 1, GL_FALSE, &world[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(prog_vf_voxel_mesh_diffusion_spec_.id,"uProjView"), 1, GL_FALSE, &proj_view[0][0]);
        glUniform3f(glGetUniformLocation(prog_vf_voxel_mesh_diffusion_spec_.id,"uViewPos"), cam_pos.x, cam_pos.y, cam_pos.z);
    };

    // Если доступно ARB_indirect_parameters -> НУЛЕВОЙ readback
    if (GLEW_ARB_indirect_parameters) {
        // drawcount берём прямо из frame_counters_.z (offset 8 байт: x,y,z,w)
        glBindBuffer(GL_PARAMETER_BUFFER, frame_counters_.id());

        const GLintptr indirectOffset = 0;
        const GLintptr countOffset    = 8; // byte offset of counters.z

        attash_shader_program();

        glMultiDrawElementsIndirectCountARB(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(indirectOffset)),
            countOffset,
            maxDraws,
            stride
        );
    }
    else {
        // Fallback: читаем только 4 байта cmdCount
        uint32_t cmdCount = 0;
        frame_counters_.read_subdata(8, &cmdCount, sizeof(uint32_t));

        attash_shader_program();

        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            (const void*)0,
            (GLsizei)cmdCount,
            stride
        );
    }

    glBindVertexArray(0);
}

void VoxelGridGPU::init_draw_buffers() {
    vbo = VBO(global_vertex_buffer_.id(), sizeof(VertexGPU) * (size_t)max_mesh_vertices_);
    ebo = EBO(global_index_buffer_.id(), sizeof(uint32_t) * (size_t)max_mesh_indices_);
    vao = VAO().init_vao();

    static VertexLayout vertex_layout;
    if (vertex_layout.attributes.size() == 0) {
        vertex_layout.add(
            "position",
            0, 4, GL_FLOAT, GL_FALSE,
            sizeof(VertexGPU),
            offsetof(VertexGPU, pos), 
            0, {0.0f, 0.0f, 0.0f, 1.0f}
        );
        vertex_layout.add(
            "color",
            1, 1, GL_UNSIGNED_INT, GL_FALSE,
            sizeof(VertexGPU),
            offsetof(VertexGPU, color), 
            0, {0xffffffffu} // белый
        );
        vertex_layout.add(
            "face",
            2, 1, GL_UNSIGNED_INT, GL_FALSE,
            sizeof(VertexGPU),
            offsetof(VertexGPU, face), 
            0, {0u} // Направление 0 (хз куда это, вверх мб?)
        );
    }

    vao.setup(vbo, ebo, vertex_layout);
}

void VoxelGridGPU::mark_all_used_chunks_as_dirty() {
    chunk_meta_.bind_base(6);
    equeued_.bind_base(7);
    dirty_list_.bind_base(8);
    frame_counters_.bind_base(5);

    prog_mark_all_user_chunks_as_dirty_.use();

    glUniform1ui(glGetUniformLocation(prog_mark_all_user_chunks_as_dirty_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_mark_all_user_chunks_as_dirty_.id, "u_set_dirty_flag_bits"), 1u);

    uint32_t groups_x = math_utils::div_up_u32(count_active_chunks, 256u);
    prog_mark_all_user_chunks_as_dirty_.dispatch_compute(groups_x, 1, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
}

void VoxelGridGPU::read_states_data(
    std::unordered_map<uint32_t, uint32_t>& meta_alloc_vb_out, 
    std::unordered_map<uint32_t, uint32_t>& meta_alloc_ib_out,
    std::unordered_map<uint32_t, uint32_t>& states_alloc_vb_out, 
    std::unordered_map<uint32_t, uint32_t>& states_alloc_ib_out
) {
    const uint32_t ST_MASK_BITS = 4;
    const uint32_t ST_FREE    = 0u;
    const uint32_t ST_ALLOC   = 1u;
    const uint32_t ST_MERGED  = 2u;
    const uint32_t ST_MERGING = 3u;
    const uint32_t ST_READY = 4u;
    const uint32_t ST_CONCEDED = 5u;
    const uint32_t ST_MASK   = (1u << ST_MASK_BITS) - 1u;

    std::vector<ChunkMeshAlloc> alloc_meta(count_active_chunks);
    chunk_mesh_alloc_.read_subdata(0, alloc_meta.data(), sizeof(ChunkMeshAlloc) * count_active_chunks);

    std::vector<uint32_t> vb_states(count_vb_pages_);
    vb_state_.read_subdata(0, vb_states.data(), sizeof(uint32_t) * count_vb_pages_);

    std::vector<uint32_t> ib_states(count_ib_pages_);
    ib_state_.read_subdata(0, ib_states.data(), sizeof(uint32_t) * count_ib_pages_);

    std::unordered_map<uint32_t, uint32_t> meta_alloc_vb, meta_alloc_ib;
    for (ChunkMeshAlloc& meta : alloc_meta) {
        if (meta.v_startPage != 0xFFFFFFFFu) meta_alloc_vb.emplace(meta.v_startPage, meta.v_order);
        if (meta.i_startPage != 0xFFFFFFFFu) meta_alloc_ib.emplace(meta.i_startPage, meta.i_order);
    }

    std::unordered_map<uint32_t, uint32_t> states_alloc_vb, states_alloc_ib;
    for (uint32_t i = 0; i < vb_states.size(); i++) {
        uint32_t kind = vb_states[i] & ST_MASK;
        uint32_t order = vb_states[i] >> ST_MASK_BITS;
        if (kind == ST_ALLOC) states_alloc_vb.emplace(i, order);
    }

    for (uint32_t i = 0; i < ib_states.size(); i++) {
        uint32_t kind = ib_states[i] & ST_MASK;
        uint32_t order = ib_states[i] >> ST_MASK_BITS;
        if (kind == ST_ALLOC) states_alloc_ib.emplace(i, order);
    }

    meta_alloc_vb_out = std::move(meta_alloc_vb);
    meta_alloc_ib_out = std::move(meta_alloc_ib);
    states_alloc_vb_out = std::move(states_alloc_vb);
    states_alloc_ib_out = std::move(states_alloc_ib);
}