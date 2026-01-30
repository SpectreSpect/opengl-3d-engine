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
    ComputeShader* k_clear_cs)
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

void VoxelRasterizatorGPU::calculate_roi_on_cpu(const MeshData& mesh_data,
                                                const glm::mat4& transform,
                                                const VertexLayout& vertex_layout,
                                                float voxel_size,
                                                int chunk_size,
                                                int pad_voxels)
{
    if (voxel_size <= 0.0f) {
        throw std::invalid_argument("calculate_roi_on_cpu: voxel_size must be > 0");
    }
    if (chunk_size <= 0) {
        throw std::invalid_argument("calculate_roi_on_cpu: chunk_size must be > 0");
    }

    // stride в float-ах (как ты делаешь в других местах)
    if (vertex_layout.attributes.empty()) {
        throw std::runtime_error("calculate_roi_on_cpu: vertex_layout.attributes is empty");
    }
    const size_t stride_f = vertex_layout.attributes[0].stride / sizeof(float);
    if (stride_f == 0) {
        throw std::runtime_error("calculate_roi_on_cpu: stride is 0");
    }
    if (mesh_data.vertices.size() % stride_f != 0) {
        throw std::runtime_error("calculate_roi_on_cpu: vertices size not multiple of stride");
    }

    int pos_attr = -1;
    try {
        pos_attr = vertex_layout.find_attribute_id_by_name("position");
    } catch (...) {
        throw std::runtime_error("calculate_roi_on_cpu: vertex_layout has no attribute named 'position'");
    }

    const size_t pos_off_f = vertex_layout.attributes[pos_attr].offset / sizeof(float);
    if (pos_off_f + 2 >= stride_f) {
        throw std::runtime_error("calculate_roi_on_cpu: position offset out of stride");
    }

    const size_t vertex_count = mesh_data.vertices.size() / stride_f;
    if (vertex_count == 0) {
        // пусто -> ROI 0
        set_roi(glm::ivec3(0), glm::uvec3(0));
        return;
    }

    // AABB в voxel-space (float)
    glm::vec3 mn(std::numeric_limits<float>::infinity());
    glm::vec3 mx(-std::numeric_limits<float>::infinity());

    for (size_t v = 0; v < vertex_count; ++v) {
        const size_t base = v * stride_f + pos_off_f;

        glm::vec4 p(mesh_data.vertices[base + 0],
                    mesh_data.vertices[base + 1],
                    mesh_data.vertices[base + 2],
                    1.0f);

        glm::vec3 pv = glm::vec3(transform * p) / voxel_size;

        mn = glm::min(mn, pv);
        mx = glm::max(mx, pv);
    }

    // Делаем инклюзивный диапазон voxel-индексов так же, как в compute:
    // vmin = floor(mn)
    // vmax = ceil(mx) - 1
    glm::ivec3 vmin = glm::ivec3(glm::floor(mn));
    glm::ivec3 vmax = glm::ivec3(glm::ceil(mx)) - glm::ivec3(1);

    // На всякий случай (если mx < mn из-за NaN или чего-то такого)
    vmax = glm::max(vmax, vmin);

    // Паддинг в вокселях (полезно против пограничных float ошибок)
    if (pad_voxels > 0) {
        vmin -= glm::ivec3(pad_voxels);
        vmax += glm::ivec3(pad_voxels);
    }

    // Переводим voxel-индексы в chunk-индексы (chunk_size в voxels)
    glm::ivec3 cmin(
        math_utils::floor_div(vmin.x, chunk_size),
        math_utils::floor_div(vmin.y, chunk_size),
        math_utils::floor_div(vmin.z, chunk_size)
    );
    glm::ivec3 cmax(
        math_utils::floor_div(vmax.x, chunk_size),
        math_utils::floor_div(vmax.y, chunk_size),
        math_utils::floor_div(vmax.z, chunk_size)
    );

    // dim = cmax - cmin + 1 (включительно)
    glm::ivec3 dim_i = cmax - cmin + glm::ivec3(1);
    dim_i = glm::max(dim_i, glm::ivec3(0)); // на всякий

    set_roi(cmin, glm::uvec3(dim_i));
}

void VoxelRasterizatorGPU::ensure_roi_buffers(size_t vertex_count, size_t tri_count, uint32_t chunk_count, uint32_t chunk_voxel_count) {
    #ifdef VOXEL_RAST_DEBUG
        const GLenum readUsage = GL_STREAM_READ;
    #else
        const GLenum readUsage = GL_DYNAMIC_DRAW; // в релизе мы почти не читаем
    #endif

    // positions vec4[]
    size_t need_pos = vertex_count * sizeof(glm::vec4);
    if (need_pos > pos_cap_bytes_) {
        positions_ssbo_ = SSBO(need_pos, GL_DYNAMIC_DRAW, nullptr);
        pos_cap_bytes_ = need_pos;
    }

    // colors vec4[]
    size_t need_col = vertex_count * sizeof(glm::vec4);
    if (need_col > col_cap_bytes_) {
        colors_ssbo_ = SSBO(need_col, GL_DYNAMIC_DRAW, nullptr);
        col_cap_bytes_ = need_col;
    }

    // tris uvec4[]
    size_t need_tris = tri_count * sizeof(glm::uvec4);
    if (need_tris > tri_cap_bytes_) {
        tris_ssbo_ = SSBO(need_tris, GL_DYNAMIC_DRAW, nullptr);
        tri_cap_bytes_ = need_tris;
    }

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

void VoxelRasterizatorGPU::upload_positions_tris_colors(const MeshData& mesh_data, const VertexLayout& layout) {
    // Вытаскиваем ТОЛЬКО позиции (vec3) + треугольники (uvec3) на CPU и заливаем в SSBO.
    // Это дешево и упрощает compute.
    const size_t stride_f = layout.attributes[0].stride / sizeof(float);
    if (stride_f == 0) throw std::runtime_error("vertex stride is 0");
    if (mesh_data.vertices.size() % stride_f != 0) throw std::runtime_error("vertices size not multiple of stride");
    if (mesh_data.indices.size() % 3 != 0) throw std::runtime_error("indices size not multiple of 3");

    const int pos_attr = layout.find_attribute_id_by_name("position");
    const size_t pos_off_f = layout.attributes[pos_attr].offset / sizeof(float);

    int col_attr = -1;
    try { col_attr = layout.find_attribute_id_by_name("color"); }
    catch (...) { col_attr = -1; }

    size_t col_off_f = 0;
    bool has_color = false;
    if (col_attr >= 0) {
        col_off_f = layout.attributes[col_attr].offset / sizeof(float);
        has_color = (col_off_f + 2 < stride_f);
    }

    if (pos_off_f + 2 >= stride_f) throw std::runtime_error("position offset out of stride");

    const size_t vertex_count = mesh_data.vertices.size() / stride_f;
    const size_t tri_count = mesh_data.indices.size() / 3;

    std::vector<glm::vec4> positions(vertex_count);
    std::vector<glm::vec4> colors(vertex_count);

    for (size_t v = 0; v < vertex_count; ++v) {
        size_t base = v * stride_f;

        positions[v] = glm::vec4(
            mesh_data.vertices[base + pos_off_f + 0],
            mesh_data.vertices[base + pos_off_f + 1],
            mesh_data.vertices[base + pos_off_f + 2],
            1.0f
        );

        if (has_color) {
            colors[v] = glm::vec4(
                mesh_data.vertices[base + col_off_f + 0],
                mesh_data.vertices[base + col_off_f + 1],
                mesh_data.vertices[base + col_off_f + 2],
                1.0f
            );
        } else {
            colors[v] = glm::vec4(1,1,1,1);
        }
    }

    std::vector<glm::uvec4> tris(tri_count);
    for (size_t t = 0; t < tri_count; ++t) {
        tris[t] = glm::uvec4(
            (uint32_t)mesh_data.indices[t * 3 + 0],
            (uint32_t)mesh_data.indices[t * 3 + 1],
            (uint32_t)mesh_data.indices[t * 3 + 2],
            0u
        );
    }

    positions_ssbo_.update_discard(positions.data(), positions.size() * sizeof(glm::vec4));
    colors_ssbo_.update_discard(colors.data(), colors.size() * sizeof(glm::vec4));
    tris_ssbo_.update_discard(tris.data(), tris.size() * sizeof(glm::uvec4));
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

    uint32_t numBlocks = div_up_u32(n, 256u);
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
    prog_add_block_offsets_.dispatch_compute(div_up_u32(n, 256u), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::gpu_exclusive_scan_u32(SSBO& in_u32, SSBO& out_u32, uint32_t n) {
    gpu_exclusive_scan_u32_impl(in_u32, out_u32, n, 0);
}

void VoxelRasterizatorGPU::rasterize(const MeshData& mesh_data,
                                    const glm::mat4& transform,
                                    const VertexLayout& vertex_layout,
                                    float voxel_size,
                                    int chunk_size) {
    prog_voxelize_.print_program_log("voxelize");
    prog_clear_.print_program_log("clear");

    // 0) ROI (пока на CPU)
    calculate_roi_on_cpu(mesh_data, transform, vertex_layout, voxel_size, chunk_size, /*pad_voxels=*/1);

    chunk_count_ = roi_chunk_count();
    if (chunk_count_ == 0 || roi_dim_.x == 0 || roi_dim_.y == 0 || roi_dim_.z == 0) {
        last_total_pairs_ = 0;
        return;
    }

    const size_t stride_f = vertex_layout.attributes[0].stride / sizeof(float);
    if (stride_f == 0 || (mesh_data.vertices.size() % stride_f) != 0 || (mesh_data.indices.size() % 3) != 0) {
        last_total_pairs_ = 0;
        return;
    }

    const size_t vertex_count = mesh_data.vertices.size() / stride_f;
    const uint32_t tri_count  = static_cast<uint32_t>(mesh_data.indices.size() / 3);
    if (vertex_count == 0 || tri_count == 0) {
        last_total_pairs_ = 0;
        return;
    }

    const uint32_t chunk_voxel_count =
        uint32_t(chunk_size) * uint32_t(chunk_size) * uint32_t(chunk_size);

    // 1) Буферы (pair_capacity пока 1, после scan расширим)
    ensure_roi_buffers(vertex_count, tri_count, chunk_count_, chunk_voxel_count);

    // 2) Upload: позиции/цвета/трисы
    upload_positions_tris_colors(mesh_data, vertex_layout);

    // 3) Очистить counters
    {
        uint32_t zero = 0;
        counters_ssbo_.bind();
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        SSBO::unbind();
    }

#if defined(VOXEL_RAST_DEBUG)
    // если count-шейдер пишет в debug_ssbo_ — очищаем (иначе можно удалить полностью)
    {
        int32_t z = 0;
        debug_ssbo_.bind();
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32I, GL_RED_INTEGER, GL_INT, &z);
        SSBO::unbind();
    }
#endif

    // 4) Pass1: COUNT (GPU)
    {
        positions_ssbo_.bind_base(0);
        tris_ssbo_.bind_base(1);
        counters_ssbo_.bind_base(2);
#if defined(VOXEL_RAST_DEBUG)
        debug_ssbo_.bind_base(3);
#endif

        prog_count_.use();

        glUniformMatrix4fv(glGetUniformLocation(prog_count_.id, "uTransform"), 1, GL_FALSE, &transform[0][0]);
        glUniform1f       (glGetUniformLocation(prog_count_.id, "uVoxelSize"), voxel_size);
        glUniform1i       (glGetUniformLocation(prog_count_.id, "uChunkSize"), chunk_size);
        glUniform3i       (glGetUniformLocation(prog_count_.id, "uChunkOrigin"), roi_origin_.x, roi_origin_.y, roi_origin_.z);
        glUniform3ui      (glGetUniformLocation(prog_count_.id, "uGridDim"), roi_dim_.x, roi_dim_.y, roi_dim_.z);
        glUniform1ui      (glGetUniformLocation(prog_count_.id, "uTriCount"), tri_count);

        const uint32_t tg = div_up_u32(tri_count, 256u);
        if (tg > 0) {
            prog_count_.dispatch_compute(tg, 1, 1);
        }

        // Важно: чтобы записи в SSBO стали видимы для readback/следующих проходов
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

#if defined(VOXEL_RAST_DEBUG)
    // (опционально) читать dbg буфер
    {
        std::vector<int32_t> dbg(32, 0);
        debug_ssbo_.read_subdata(0, dbg.data(), dbg.size() * sizeof(int32_t));

        std::cout
            << "dbg trianglesProcessed=" << dbg[0] << "\n"
            << "dbg wroteOnce=" << dbg[1] << "\n"
            << "vmin=" << dbg[2] << "," << dbg[3] << "," << dbg[4] << "\n"
            << "vmax=" << dbg[5] << "," << dbg[6] << "," << dbg[7] << "\n"
            << "cmin=" << dbg[8] << "," << dbg[9] << "," << dbg[10] << "\n"
            << "cmax=" << dbg[11] << "," << dbg[12] << "," << dbg[13] << "\n"
            << "roiOrigin=" << dbg[14] << "," << dbg[15] << "," << dbg[16] << "\n"
            << "addsInROI=" << dbg[17] << "\n"
            << "gridDim=" << dbg[18] << "," << dbg[19] << "," << dbg[20] << "\n";
    }
#endif

    // ---------- Pass2 (CPU): counters -> offsets/cursor (debugged) ----------

    // 1) Read counters from GPU
    std::vector<uint32_t> counters(chunk_count_);
    counters_ssbo_.read_subdata(0, counters.data(),
                            counters.size() * sizeof(uint32_t));

    // 2) Quick stats
    uint64_t sum = 0;
    uint32_t mx = 0;
    uint32_t nonzero = 0;
    for (uint32_t v : counters) {
        sum += v;
        mx = std::max(mx, v);
        if (v) ++nonzero;
    }

    std::cout << "[CPU scan] counters: sum=" << sum
            << " max=" << mx
            << " nonzero=" << nonzero
            << " chunk_count=" << chunk_count_ << "\n";

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
    std::cout << "[CPU scan] totalPairs=" << last_total_pairs_ << "\n";

    // 4) Invariants
    if (offsets[0] != 0) {
        throw std::runtime_error("offsets[0] != 0");
    }
    if (offsets.back() != last_total_pairs_) {
        throw std::runtime_error("offsets[last] != totalPairs");
    }
    for (uint32_t i = 0; i < chunk_count_; ++i) {
        if (offsets[i] > offsets[i + 1]) {
            throw std::runtime_error("offsets not monotonic");
        }
        if ((offsets[i + 1] - offsets[i]) != counters[i]) {
            throw std::runtime_error("offsets diff != counters[i]");
        }
    }

    // 5) Print first few nonzero chunks (optional but useful)
    auto idx_to_chunk = [&](uint32_t idx) -> glm::ivec3 {
        uint32_t x = idx % roi_dim_.x;
        uint32_t y = (idx / roi_dim_.x) % roi_dim_.y;
        uint32_t z = idx / (roi_dim_.x * roi_dim_.y);
        return roi_origin_ + glm::ivec3((int)x, (int)y, (int)z);
    };

    int printed = 0;
    for (uint32_t i = 0; i < chunk_count_ && printed < 10; ++i) {
        if (counters[i] == 0) continue;
        glm::ivec3 c = idx_to_chunk(i);
        std::cout << "  nonzero[" << printed << "]: idx=" << i
                << " cnt=" << counters[i]
                << " chunk=(" << c.x << "," << c.y << "," << c.z << ")\n";
        ++printed;
    }

    // 6) If empty -> exit early
    if (last_total_pairs_ == 0) {
        return;
    }

    // 8) Upload offsets/cursor/totalPairs to GPU
    offsets_ssbo_.update_discard(offsets.data(), offsets.size() * sizeof(uint32_t));
    cursor_ssbo_.update_discard(offsets.data(), chunk_count_ * sizeof(uint32_t));
    total_pairs_ssbo_.update_discard(&last_total_pairs_, sizeof(uint32_t));

    // 9) Verify offsets[last] on GPU
    uint32_t gpu_last = 0;
    offsets_ssbo_.read_subdata(chunk_count_ * sizeof(uint32_t), &gpu_last, sizeof(uint32_t));

    std::cout << "[GPU verify] offsets[last]=" << gpu_last
            << " expected=" << last_total_pairs_ << "\n";

    if (gpu_last != last_total_pairs_) {
        throw std::runtime_error("GPU offsets[last] mismatch after upload");
    }

    std::vector<uint32_t> active_chunks;
    active_chunks.reserve(nonzero);

    for (uint32_t i = 0; i < chunk_count_; ++i) {
        if (counters[i] != 0) active_chunks.push_back(i);
    }

    uint32_t activeCount = (uint32_t)active_chunks.size();
    if (activeCount == 0) return;

    ensure_active_chunk_buffers(chunk_voxel_count, last_total_pairs_, activeCount);

    active_chunks_ssbo_.update_discard(active_chunks.data(), active_chunks.size() * sizeof(uint32_t));

// ---------- end Pass2 ----------




    // Pass 3: fill triangleIndices
    positions_ssbo_.bind_base(0);
    tris_ssbo_.bind_base(1);
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

    const uint32_t tg_fill = div_up_u32(tri_count, 256u);
    if (tg_fill > 0) {
        prog_fill_.dispatch_compute(tg_fill, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    //#################################DEBUG#############################
    // DEBUG: verify cursor end == offsets[i+1] for some chunks
    {
        std::vector<uint32_t> cursor_dbg(chunk_count_);
        cursor_ssbo_.read_subdata(0, cursor_dbg.data(), cursor_dbg.size() * sizeof(uint32_t));

        int bad = 0;
        for (uint32_t i = 0; i < chunk_count_; ++i) {
            if (cursor_dbg[i] != offsets[i + 1]) {
                if (++bad <= 10) {
                    std::cout << "cursor mismatch at " << i
                            << ": got=" << cursor_dbg[i]
                            << " expected=" << offsets[i+1] << "\n";
                }
            }
        }
        std::cout << "cursor mismatches: " << bad << "\n";
        if (bad) throw std::runtime_error("fill pass failed: cursor != offsets[i+1]");
    }
    //#################################DEBUG#############################

    std::vector<uint32_t> triIdsDbg(last_total_pairs_);
    tri_indices_ssbo_.read_subdata(0, triIdsDbg.data(), triIdsDbg.size() * 4);

    uint32_t mxTid = 0;
    for (auto v: triIdsDbg) mxTid = std::max(mxTid, v);
    std::cout << "triIds max = " << mxTid << " (tri_count=" << tri_count << ")\n";

    
    // На этом этапе на GPU готов CSR:
    // offsets_ssbo_ (uint[chunkCount+1])
    // tri_indices_ssbo_ (uint[totalPairs])
    //
    // Дальше Pass 4: "один workgroup на чанк" и растеризация по списку треугольников.
    const uint32_t total_voxels = chunk_count_ * chunk_voxel_count;

    const uint32_t chunkVoxelCount = chunk_size * chunk_size * chunk_size;

    // 16 для chunk_size=16 и local_size_x=256
    uint32_t groupsX = div_up_u32(chunk_voxel_count, 256u); // 4096/256 = 16
    uint32_t groupsY = activeCount;
    uint32_t groupsZ = 1;


    positions_ssbo_.bind_base(0);
    colors_ssbo_.bind_base(1);
    tris_ssbo_.bind_base(2);
    offsets_ssbo_.bind_base(3);
    tri_indices_ssbo_.bind_base(4);
    voxels_ssbo_.bind_base(5);
    active_chunks_ssbo_.bind_base(6);

    prog_clear_.use();
    glUniform1ui(glGetUniformLocation(prog_clear_.id, "uChunkVoxelCount"), chunkVoxelCount);
    glUniform1ui(glGetUniformLocation(prog_clear_.id, "uActiveCount"), activeCount);
    prog_clear_.dispatch_compute(groupsX, activeCount, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    prog_voxelize_.use();
    glUniformMatrix4fv(glGetUniformLocation(prog_voxelize_.id, "uTransform"), 1, GL_FALSE, &transform[0][0]);
    glUniform1f(glGetUniformLocation(prog_voxelize_.id, "uVoxelSize"), voxel_size);
    glUniform1i(glGetUniformLocation(prog_voxelize_.id, "uChunkSize"), chunk_size);
    glUniform3i(glGetUniformLocation(prog_voxelize_.id, "uChunkOrigin"), roi_origin_.x, roi_origin_.y, roi_origin_.z);
    glUniform3ui(glGetUniformLocation(prog_voxelize_.id, "uGridDim"), roi_dim_.x, roi_dim_.y, roi_dim_.z);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uChunkVoxelCount"), chunkVoxelCount);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uChunkCount"), chunk_count_);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uActiveCount"), activeCount);
    glUniform1ui(glGetUniformLocation(prog_voxelize_.id, "uTriCount"), tri_count);

    auto drain = [](){ while (glGetError()!=GL_NO_ERROR){} };
    drain();
    prog_voxelize_.dispatch_compute(groupsX, groupsY, groupsZ);
    GLenum e = glGetError();
    std::cout << "err(voxelize dispatch)=" << e << "\n";
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish(); // только для дебага

    //#################################DEBUG#############################
    // uint32_t someNonZeroChunkIdx = 43358;        // возьми из лога CPU scan
    // uint32_t base = someNonZeroChunkIdx * chunk_voxel_count;

    // std::vector<uint32_t> vox(chunk_voxel_count);
    // voxels_ssbo_.read_subdata(base*sizeof(uint32_t), vox.data(), vox.size()*sizeof(uint32_t));

    // nonzero = 0;
    // for (auto v: vox) if (v) ++nonzero;
    // std::cout << "chunk " << someNonZeroChunkIdx << " nonzero voxels=" << nonzero << "/" << vox.size() << "\n";
    //#################################DEBUG#############################


    std::vector<uint32_t> active(activeCount);
    active_chunks_ssbo_.read_subdata(0, active.data(), activeCount * sizeof(uint32_t));

    std::vector<uint32_t> vox(activeCount * chunk_voxel_count);
    voxels_ssbo_.read_subdata(0, vox.data(), vox.size() * sizeof(uint32_t));

    // helper: dense idx -> chunk coord
    auto dense_to_chunk = [&](uint32_t idx) -> glm::ivec3 {
        uint32_t x = idx % roi_dim_.x;
        uint32_t y = (idx / roi_dim_.x) % roi_dim_.y;
        uint32_t z = idx / (roi_dim_.x * roi_dim_.y);
        return roi_origin_ + glm::ivec3((int)x,(int)y,(int)z);
    };

    std::vector<glm::ivec3> voxel_positions;
    voxel_positions.reserve(activeCount * chunk_voxel_count);
    for (uint32_t a = 0; a < activeCount; ++a) {
        glm::ivec3 cpos = dense_to_chunk(active[a]); // chunk coords in world-chunk-space

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

    glm::ivec3 mn_ = glm::ivec3(INT_MAX);
    glm::ivec3 mx_ = glm::ivec3(INT_MIN);
    for (auto p : voxel_positions) { 
        mn_ = glm::min(mn_, p); 
        mx_ = glm::max(mx_, p); 
    }
    std::cout << "voxel_positions AABB: "
            << mn_.x<<","<<mn_.y<<","<<mn_.z<<" .. "
            << mx_.x<<","<<mx_.y<<","<<mx_.z<<"\n";

    std::vector<Voxel> voxels;
    voxels.resize(voxel_positions.size());
    for (int i = 0; i < voxels.size(); i++) {
        Voxel voxel;
        voxel.visible = true;
        voxel.color = glm::vec3(1.0f, 0.0f, 0.0f);

        voxels[i] = std::move(voxel);
    }

    std::cout << "voxel_positions.size(): " << voxel_positions.size() << std::endl;
    std::cout << "voxels.size(): " << voxels.size() << std::endl;


    gridable->set_voxels(voxels, voxel_positions);
}