#include "voxel_rasterizator_gpu.h"

VoxelRasterizatorGPU::VoxelRasterizatorGPU(
    Gridable* gridable, 
    ComputeShader* k_count_cs, 
    ComputeShader* k_scan_blocks_cs,
    ComputeShader* k_add_block_offsets_cs,
    ComputeShader* k_fix_last_cs,
    ComputeShader* k_copy_offsets_to_cursor_cs,
    ComputeShader* k_fill_cs,
    ComputeShader* k_voxelize_cs,
    ComputeShader* k_clear_cs,
    ComputeShader* k_roi_reduce_indices_cs,
    ComputeShader* k_roi_reduce_pairs_cs,
    ComputeShader* k_roi_finalize_cs)
{
    this->gridable = gridable;
    prog_count_ = ComputeProgram(k_count_cs);
    prog_scan_blocks_ = ComputeProgram(k_scan_blocks_cs);
    prog_add_block_offsets_ = ComputeProgram(k_add_block_offsets_cs);
    prog_fix_last_ = ComputeProgram(k_fix_last_cs);
    prog_copy_offsets_to_cursor_ = ComputeProgram(k_copy_offsets_to_cursor_cs);
    prog_fill_ = ComputeProgram(k_fill_cs);
    prog_voxelize_ = ComputeProgram(k_voxelize_cs);
    prog_clear_ = ComputeProgram(k_clear_cs);
    prog_roi_reduce_indices_ = ComputeProgram(k_roi_reduce_indices_cs);
    prog_roi_reduce_pairs_ = ComputeProgram(k_roi_reduce_pairs_cs);
    prog_roi_finalize_ = ComputeProgram(k_roi_finalize_cs);

    // минимальные буферы (чтобы SSBO(0) был валиден) — если твой SSBO(0) не поддерживает,
    // замени на SSBO(4, nullptr, usage) и т.п.
    total_pairs_ssbo_ = SSBO(sizeof(uint32_t), GL_DYNAMIC_DRAW, nullptr);
    active_chunks_ssbo_ = SSBO(sizeof(uint32_t), GL_DYNAMIC_DRAW, nullptr);
    active_cap_bytes_ = sizeof(uint32_t);
}

VoxelRasterizatorGPU::~VoxelRasterizatorGPU() {

}

void VoxelRasterizatorGPU::set_roi(glm::ivec3 chunk_origin, glm::uvec3 grid_dim) {
    roi_origin_ = chunk_origin;
    roi_dim_ = grid_dim;
}

void VoxelRasterizatorGPU::ensure_roi_reduce_level(uint32_t level, uint32_t numPairs) {
    size_t need = size_t(numPairs) * 2 * sizeof(glm::vec4);
    if (roi_reduce_levels_.size() <= level) {
        roi_reduce_levels_.resize(level + 1);
        roi_reduce_caps_.resize(level + 1, 0);
    }
    
    if (need > roi_reduce_caps_[level]) {
        roi_reduce_levels_[level] = SSBO(need, GL_DYNAMIC_DRAW, nullptr);
        roi_reduce_caps_[level] = need;
    }

    size_t roi_out_need = sizeof(int) * 4 + sizeof(uint32_t) * 4; // 32 bytes
    if (roi_out_need > roi_out_cap_bytes_) {
        roi_out_ssbo_ = SSBO(roi_out_need, GL_DYNAMIC_DRAW, nullptr);
        roi_out_cap_bytes_ = roi_out_need;
    }
}

void VoxelRasterizatorGPU::calculate_roi(const Mesh& mesh, float voxel_size, int chunk_size, int pad_voxels) {
    glm::mat4 transform = mesh.get_model_matrix();

    uint32_t indexCount = uint32_t(mesh.ebo->size_bytes / sizeof(uint32_t));
    if (indexCount == 0) { set_roi({0,0,0}, {0,0,0}); return; }

    const uint32_t strideF = mesh.vertex_layout->attributes[0].stride / sizeof(float);
    const int posAttr = mesh.vertex_layout->find_attribute_id_by_name("position");
    const uint32_t posOffF = mesh.vertex_layout->attributes[posAttr].offset / sizeof(float);

    // level 0: indices -> group AABB
    uint32_t numPairs = math_utils::div_up_u32(indexCount, 256u);
    ensure_roi_reduce_level(0, numPairs);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vbo->id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo->id);
    roi_reduce_levels_[0].bind_base(2);

    prog_roi_reduce_indices_.use();
    glUniformMatrix4fv(glGetUniformLocation(prog_roi_reduce_indices_.id, "uTransform"), 1, GL_FALSE, &transform[0][0]);
    glUniform1f (glGetUniformLocation(prog_roi_reduce_indices_.id, "uVoxelSize"), voxel_size);
    glUniform1ui(glGetUniformLocation(prog_roi_reduce_indices_.id, "uIndexCount"), indexCount);
    glUniform1ui(glGetUniformLocation(prog_roi_reduce_indices_.id, "uStrideF"), strideF);
    glUniform1ui(glGetUniformLocation(prog_roi_reduce_indices_.id, "uPosOffF"), posOffF);

    prog_roi_reduce_indices_.dispatch_compute(numPairs, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // levels: reduce pairs -> reduce pairs -> ...
    uint32_t level = 0;
    while (numPairs > 1) {
        uint32_t nextPairs = math_utils::div_up_u32(numPairs, 256u);
        ensure_roi_reduce_level(level + 1, nextPairs);

        roi_reduce_levels_[level].bind_base(0);
        roi_reduce_levels_[level + 1].bind_base(1);

        prog_roi_reduce_pairs_.use();
        glUniform1ui(glGetUniformLocation(prog_roi_reduce_pairs_.id, "uPairCount"), numPairs);
        prog_roi_reduce_pairs_.dispatch_compute(nextPairs, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        numPairs = nextPairs;
        level++;
    }

    // finalize
    roi_reduce_levels_[level].bind_base(0);
    roi_out_ssbo_.bind_base(1);

    prog_roi_finalize_.use();
    glUniform1i(glGetUniformLocation(prog_roi_finalize_.id, "uChunkSize"), chunk_size);
    glUniform1i(glGetUniformLocation(prog_roi_finalize_.id, "uPadVoxels"), pad_voxels);
    glUniform1f(glGetUniformLocation(prog_roi_finalize_.id, "uEps"), 1e-4f);
    prog_roi_finalize_.dispatch_compute(1,1,1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // readback 32 байта
    struct RoiGPU { glm::ivec4 origin; glm::uvec4 dim; } roi{};
    roi_out_ssbo_.read_subdata(0, &roi, sizeof(RoiGPU));
    set_roi(glm::ivec3(roi.origin), glm::uvec3(roi.dim));
}


void VoxelRasterizatorGPU::ensure_roi_buffers(size_t vertex_count, size_t tri_count, uint32_t chunk_count, uint32_t chunk_voxel_count) {
    #ifdef VOXEL_RAST_DEBUG
        const GLenum readUsage = GL_STREAM_READ;
    #else
        const GLenum readUsage = GL_DYNAMIC_DRAW; // в релизе мы почти не читаем
    #endif

    // counters uint[chunk_count] GPU->CPU
    size_t need_counters = (size_t)chunk_count * sizeof(uint32_t);
    if (need_counters > counters_cap_bytes_) {
        counters_ssbo_ = SSBO(need_counters, readUsage, nullptr);
        counters_cap_bytes_ = need_counters;
    }

    // offsets uint[chunk_count+1]
    size_t need_offsets = ((size_t)chunk_count + 1) * sizeof(uint32_t);
    if (need_offsets > offsets_cap_bytes_) {
        offsets_ssbo_ = SSBO(need_offsets, GL_DYNAMIC_DRAW, nullptr);
        offsets_cap_bytes_ = need_offsets;
    }

    // cursor uint[chunk_count] GPU->CPU (только дебаг)
    size_t need_cursor = (size_t)chunk_count * sizeof(uint32_t);
    if (need_cursor > cursor_cap_bytes_) {
        cursor_ssbo_ = SSBO(need_cursor, readUsage, nullptr);
        cursor_cap_bytes_ = need_cursor;
    }
}

void VoxelRasterizatorGPU::ensure_active_chunk_buffers(uint32_t chunk_voxel_count, uint32_t pair_capacity, uint32_t activeCount) {
    #ifdef VOXEL_RAST_DEBUG
        const GLenum readUsage = GL_STREAM_READ;
    #else
        const GLenum readUsage = GL_DYNAMIC_DRAW; // в релизе мы почти не читаем
    #endif

    // triangleIndices uint[pair_capacity]
    size_t need_pairs = (size_t)pair_capacity * sizeof(uint32_t);
    if (need_pairs > tri_indices_cap_bytes_) {
        tri_indices_ssbo_ = SSBO(need_pairs, GL_DYNAMIC_DRAW, nullptr);
        tri_indices_cap_bytes_ = need_pairs;
    }

    // voxels uint[chunk_count * chunk_voxel_count] GPU->CPU (только дебаг)
    const uint64_t total_vox = uint64_t(activeCount) * uint64_t(chunk_voxel_count);
    size_t need_vox = size_t(total_vox * sizeof(uint32_t));
    if (need_vox > vox_cap_bytes_) {
        voxels_ssbo_ = SSBO(need_vox, readUsage, nullptr);
        vox_cap_bytes_ = need_vox;
    }

    size_t need_active = std::max<size_t>(1, activeCount) * sizeof(uint32_t);
    if (need_active > active_cap_bytes_) {
        active_chunks_ssbo_ = SSBO(need_active, GL_DYNAMIC_DRAW, nullptr);
        active_cap_bytes_ = need_active;
    }
}

void VoxelRasterizatorGPU::ensure_scan_level(uint32_t level, uint32_t numBlocks) {
    if (scan_sums_.size() <= level) {
        scan_sums_.resize(level + 1);
        scan_prefix_.resize(level + 1);
        scan_caps_.resize(level + 1, 0);
    }
    size_t need = (size_t)numBlocks * sizeof(uint32_t);
    if (need > scan_caps_[level]) {
        scan_sums_[level] = SSBO(need, GL_DYNAMIC_DRAW, nullptr);
        scan_prefix_[level] = SSBO(need, GL_DYNAMIC_DRAW, nullptr);
        scan_caps_[level] = need;
    }
}

void VoxelRasterizatorGPU::gpu_exclusive_scan_u32_impl(SSBO& in_u32, SSBO& out_u32, uint32_t n, uint32_t level) {
    if (n == 0) return;

    uint32_t numBlocks = math_utils::div_up_u32(n, 256u);
    ensure_scan_level(level, numBlocks);

    // 1) scan blocks: out = exclusive scan(in), sums = per-block sum
    in_u32.bind_base(0);
    out_u32.bind_base(1);
    scan_sums_[level].bind_base(2);

    prog_scan_blocks_.use();
    glUniform1ui(glGetUniformLocation(prog_scan_blocks_.id, "uN"), n);
    prog_scan_blocks_.dispatch_compute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    if (numBlocks <= 1) return;

    // 2) scan sums -> prefix (recursive)
    gpu_exclusive_scan_u32_impl(scan_sums_[level], scan_prefix_[level], numBlocks, level + 1);

    // 3) add prefix[block] to out
    out_u32.bind_base(0);
    scan_prefix_[level].bind_base(1);

    prog_add_block_offsets_.use();
    glUniform1ui(glGetUniformLocation(prog_add_block_offsets_.id, "uN"), n);
    prog_add_block_offsets_.dispatch_compute(math_utils::div_up_u32(n, 256u), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::gpu_exclusive_scan_u32(SSBO& in_u32, SSBO& out_u32, uint32_t n) {
    gpu_exclusive_scan_u32_impl(in_u32, out_u32, n, 0);
}

glm::ivec3 VoxelRasterizatorGPU::idx_to_chunk(uint32_t idx) {
    uint32_t x = idx % roi_dim_.x;
    uint32_t y = (idx / roi_dim_.x) % roi_dim_.y;
    uint32_t z = idx / (roi_dim_.x * roi_dim_.y);
    return roi_origin_ + glm::ivec3((int)x, (int)y, (int)z);
}

void VoxelRasterizatorGPU::clear_counters() {
    uint32_t zero = 0;
    counters_ssbo_.bind();
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    SSBO::unbind();
}

void VoxelRasterizatorGPU::count_triangles_in_chunks(
    const Mesh& mesh,
    float voxel_size, 
    int chunk_size, 
    uint32_t tri_count) 
{
    glm::mat4 transform = mesh.get_model_matrix();

    const uint32_t vertex_stride_f = mesh.vertex_layout->attributes[0].stride / sizeof(float);
    const int pos_attr_id = mesh.vertex_layout->find_attribute_id_by_name("position");
    const uint32_t pos_offset_f = mesh.vertex_layout->attributes[pos_attr_id].offset / sizeof(float);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vbo->id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo->id);
    counters_ssbo_.bind_base(2);

    prog_count_.use();
    glUniformMatrix4fv(glGetUniformLocation(prog_count_.id, "uTransform"), 1, GL_FALSE, &transform[0][0]);
    glUniform1f       (glGetUniformLocation(prog_count_.id, "uVoxelSize"), voxel_size);
    glUniform1i       (glGetUniformLocation(prog_count_.id, "uChunkSize"), chunk_size);
    glUniform3i       (glGetUniformLocation(prog_count_.id, "uChunkOrigin"), roi_origin_.x, roi_origin_.y, roi_origin_.z);
    glUniform3ui      (glGetUniformLocation(prog_count_.id, "uGridDim"), roi_dim_.x, roi_dim_.y, roi_dim_.z);
    glUniform1ui      (glGetUniformLocation(prog_count_.id, "uTriCount"), tri_count);
    glUniform1ui(glGetUniformLocation(prog_count_.id, "uStrideF"), vertex_stride_f);
    glUniform1ui(glGetUniformLocation(prog_count_.id, "uPosOffF"), pos_offset_f);

    const uint32_t tg = math_utils::div_up_u32(tri_count, 256u);
    if (tg > 0) {
        prog_count_.dispatch_compute(tg, 1, 1);
    }

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::fill_triangle_indices(
    const Mesh& mesh,
    float voxel_size, 
    int chunk_size, 
    size_t tri_count
) {
    glm::mat4 transform = mesh.get_model_matrix();

    const uint32_t vertex_stride_f = mesh.vertex_layout->attributes[0].stride / sizeof(float);
    const int pos_attr_id = mesh.vertex_layout->find_attribute_id_by_name("position");
    const uint32_t pos_offset_f = mesh.vertex_layout->attributes[pos_attr_id].offset / sizeof(float);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vbo->id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo->id);
    cursor_ssbo_.bind_base(2);
    tri_indices_ssbo_.bind_base(3);

    prog_fill_.use();
    glUniformMatrix4fv(glGetUniformLocation(prog_fill_.id, "uTransform"), 1, GL_FALSE, &transform[0][0]);
    glUniform1f(glGetUniformLocation(prog_fill_.id, "uVoxelSize"), voxel_size);
    glUniform1i(glGetUniformLocation(prog_fill_.id, "uChunkSize"), chunk_size);
    glUniform3i(glGetUniformLocation(prog_fill_.id, "uChunkOrigin"), roi_origin_.x, roi_origin_.y, roi_origin_.z);
    glUniform3ui(glGetUniformLocation(prog_fill_.id, "uGridDim"), roi_dim_.x, roi_dim_.y, roi_dim_.z);
    glUniform1ui(glGetUniformLocation(prog_fill_.id, "uTriCount"), tri_count);
    glUniform1ui(glGetUniformLocation(prog_fill_.id, "uOutCapacity"), last_total_pairs_);
    glUniform1ui(glGetUniformLocation(prog_fill_.id, "uStrideF"), vertex_stride_f);
    glUniform1ui(glGetUniformLocation(prog_fill_.id, "uPosOffF"), pos_offset_f);

    const uint32_t tg_fill = math_utils::div_up_u32(tri_count, 256u);
    if (tg_fill > 0) {
        prog_fill_.dispatch_compute(tg_fill, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
}

void VoxelRasterizatorGPU::clear_active_voxels(int chunk_size, uint32_t active_count) {
    const uint32_t chunkVoxelCount = chunk_size * chunk_size * chunk_size;
    const uint32_t total_voxels = chunk_count_ * chunkVoxelCount;
    

    // 16 для chunk_size=16 и local_size_x=256
    uint32_t groupsX = math_utils::div_up_u32(chunkVoxelCount, 256u); // 4096/256 = 16
    uint32_t groupsY = active_count;
    uint32_t groupsZ = 1;

    voxels_ssbo_.bind_base(0);

    prog_clear_.use();
    glUniform1ui(glGetUniformLocation(prog_clear_.id, "uChunkVoxelCount"), chunkVoxelCount);
    glUniform1ui(glGetUniformLocation(prog_clear_.id, "uActiveCount"), active_count);
    prog_clear_.dispatch_compute(groupsX, groupsY, groupsZ);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::voxelize_chunks(
    const Mesh& mesh,
    float voxel_size, 
    int chunk_size, 
    uint32_t active_count, 
    uint32_t tri_count
) {
    const uint32_t chunk_voxel_count = chunk_size * chunk_size * chunk_size;
    const uint32_t total_voxels = chunk_count_ * chunk_voxel_count;

    glm::mat4 transform = mesh.get_model_matrix();

    const uint32_t vertex_stride_f = mesh.vertex_layout->attributes[0].stride / sizeof(float);
    const int pos_attr_id = mesh.vertex_layout->find_attribute_id_by_name("position");
    const uint32_t pos_offset_f = mesh.vertex_layout->attributes[pos_attr_id].offset / sizeof(float);

    // 16 для chunk_size=16 и local_size_x=256
    uint32_t groupsX = math_utils::div_up_u32(chunk_voxel_count, 256u); // 4096/256 = 16
    uint32_t groupsY = active_count;
    uint32_t groupsZ = 1;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mesh.vbo->id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mesh.ebo->id);
    offsets_ssbo_.bind_base(2);
    tri_indices_ssbo_.bind_base(3);
    voxels_ssbo_.bind_base(4);
    active_chunks_ssbo_.bind_base(5);
    
    prog_voxelize_.use();
    glUniformMatrix4fv(glGetUniformLocation(prog_voxelize_.id, "uTransform"), 1, GL_FALSE, &transform[0][0]);
    glUniform1f(glGetUniformLocation(prog_voxelize_.id, "uVoxelSize"), voxel_size);
    glUniform1i(glGetUniformLocation(prog_voxelize_.id, "uChunkSize"), chunk_size);
    glUniform3i(glGetUniformLocation(prog_voxelize_.id, "uChunkOrigin"), roi_origin_.x, roi_origin_.y, roi_origin_.z);
    glUniform3ui(glGetUniformLocation(prog_voxelize_.id, "uGridDim"), roi_dim_.x, roi_dim_.y, roi_dim_.z);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uChunkVoxelCount"), chunk_voxel_count);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uChunkCount"), chunk_count_);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uActiveCount"), active_count);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uTriCount"), tri_count);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "vertex_stride_f"), vertex_stride_f);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "pos_offset_f"), pos_offset_f);

    prog_voxelize_.dispatch_compute(groupsX, groupsY, groupsZ);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::rasterize(const Mesh& mesh,
                                    float voxel_size,
                                    int chunk_size) {
    prog_voxelize_.print_program_log("voxelize");
    prog_clear_.print_program_log("clear");

    // 0) ROI (пока на CPU)
    calculate_roi(mesh, voxel_size, chunk_size, 1);

    std::cout << "roi_origin_: (" << roi_origin_.x << ", " << roi_origin_.y << ", " << roi_origin_.z << ")" << std::endl;
    std::cout << "roi_dim: (" << roi_dim_.x << ", " << roi_dim_.y << ", " << roi_dim_.z << ")" << std::endl;

    chunk_count_ = roi_dim_.x * roi_dim_.y * roi_dim_.z;
    if (chunk_count_ == 0 || roi_dim_.x == 0 || roi_dim_.y == 0 || roi_dim_.z == 0) {
        last_total_pairs_ = 0;
        return;
    }

    const size_t stride_f = mesh.vertex_layout->attributes[0].stride / sizeof(float);

    const size_t vertex_count = mesh.vbo->size_bytes / (sizeof(float) * stride_f);
    const uint32_t tri_count  = mesh.ebo->size_bytes / (sizeof(uint32_t) * 3);
    if (vertex_count == 0 || tri_count == 0) {
        last_total_pairs_ = 0;
        return;
    }

    const uint32_t chunk_voxel_count = uint32_t(chunk_size) * uint32_t(chunk_size) * uint32_t(chunk_size);

    // 1) Буферы (pair_capacity пока 1, после scan расширим)
    ensure_roi_buffers(vertex_count, tri_count, chunk_count_, chunk_voxel_count);

    // 3) Очистить counters
    clear_counters();

    // 4) Pass1: COUNT (GPU)
    count_triangles_in_chunks(mesh, voxel_size, chunk_size, tri_count);

    // ---------- Pass2 (CPU): counters -> offsets/cursor (debugged) ----------

    // 1) Read counters from GPU
    std::vector<uint32_t> counters(chunk_count_);
    counters_ssbo_.read_subdata(0, counters.data(),
                            counters.size() * sizeof(uint32_t));

    // 2) Quick stats
    uint32_t nonzero = 0;
    for (uint32_t v : counters) {
        if (v) ++nonzero;
    }

    // 3) Exclusive prefix sum -> offsets (chunk_count_ + 1)
    std::vector<uint32_t> offsets(chunk_count_ + 1);
    offsets[0] = 0;

    uint64_t pref = 0;
    for (uint32_t i = 0; i < chunk_count_; ++i) {
        pref += counters[i];

        if (pref > uint64_t(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("totalPairs overflow uint32 (shrink ROI or use uint64 offsets)");
        }

        offsets[i + 1] = (uint32_t)pref;
    }

    last_total_pairs_ = (uint32_t)pref;

    // 5) Print first few nonzero chunks (optional but useful)


    // 6) If empty -> exit early
    if (last_total_pairs_ == 0) {
        return;
    }

    std::vector<uint32_t> active_chunks;
    active_chunks.reserve(nonzero);

    for (uint32_t i = 0; i < chunk_count_; ++i) {
        if (counters[i] != 0) active_chunks.push_back(i);
    }

    uint32_t activeCount = (uint32_t)active_chunks.size();
    if (activeCount == 0) return;

    ensure_active_chunk_buffers(chunk_voxel_count, last_total_pairs_, activeCount);

    // 8) Upload offsets/cursor/totalPairs to GPU
    active_chunks_ssbo_.update_discard(active_chunks.data(), active_chunks.size() * sizeof(uint32_t));
    offsets_ssbo_.update_discard(offsets.data(), offsets.size() * sizeof(uint32_t));
    cursor_ssbo_.update_discard(offsets.data(), chunk_count_ * sizeof(uint32_t));
    total_pairs_ssbo_.update_discard(&last_total_pairs_, sizeof(uint32_t));

// ---------- end Pass2 ----------
    // Pass 3: fill triangleIndices
    fill_triangle_indices(mesh, voxel_size, chunk_size, tri_count);

    // На этом этапе на GPU готов CSR:
    // offsets_ssbo_ (uint[chunkCount+1])
    // tri_indices_ssbo_ (uint[totalPairs])
    //
    // Дальше Pass 4: "один workgroup на чанк" и растеризация по списку треугольников.
    clear_active_voxels(chunk_size, activeCount);
    voxelize_chunks(mesh, voxel_size, chunk_size, activeCount, tri_count);

    // Pass 5: Считывание данных
    std::vector<uint32_t> active(activeCount);
    active_chunks_ssbo_.read_subdata(0, active.data(), activeCount * sizeof(uint32_t));

    std::vector<uint32_t> vox(activeCount * chunk_voxel_count);
    voxels_ssbo_.read_subdata(0, vox.data(), vox.size() * sizeof(uint32_t));

    std::vector<glm::ivec3> voxel_positions;
    voxel_positions.reserve(activeCount * chunk_voxel_count);
    for (uint32_t a = 0; a < activeCount; ++a) {
        glm::ivec3 cpos = idx_to_chunk(active[a]); // chunk coords in world-chunk-space

        const uint32_t* src = vox.data() + size_t(a) * chunk_voxel_count;

        for (uint32_t i = 0; i < chunk_voxel_count; ++i) {
            uint32_t packed = src[i];
            if (packed != 0) {
                uint32_t lvp_x = i % chunk_size;
                uint32_t lvp_y = (i / chunk_size) % chunk_size;
                uint32_t lvp_z = i / (chunk_size * chunk_size);
                glm::ivec3 voxel_pos = cpos * chunk_size + glm::ivec3(lvp_x, lvp_y, lvp_z);
                voxel_positions.push_back(voxel_pos);
            }
        }
    }

    std::vector<Voxel> voxels;
    voxels.resize(voxel_positions.size());
    for (int i = 0; i < voxels.size(); i++) {
        Voxel voxel;
        voxel.visible = true;
        voxel.color = glm::vec3(1.0f, 0.0f, 0.0f);

        voxels[i] = std::move(voxel);
    }

    gridable->set_voxels(voxels, voxel_positions);
}
