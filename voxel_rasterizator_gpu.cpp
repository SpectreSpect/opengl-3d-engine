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

// void VoxelRasterizatorGPU::calculate_roi_on_cpu(
//     const Mesh& mesh,
//     float voxel_size,
//     int chunk_size,
//     int pad_voxels)
// {
//     auto finite3 = [](const glm::vec3& v) {
//         return !glm::any(glm::isnan(v)) && !glm::any(glm::isinf(v));
//     };


//     if (voxel_size <= 0.0f) throw std::invalid_argument("voxel_size must be > 0");
//     if (chunk_size <= 0)    throw std::invalid_argument("chunk_size must be > 0");
//     if (vertex_layout.attributes.empty()) throw std::runtime_error("vertex_layout.attributes is empty");

//     const size_t stride_f = vertex_layout.attributes[0].stride / sizeof(float);
//     if (stride_f == 0) throw std::runtime_error("stride is 0");
//     if (mesh_data.vertices.size() % stride_f != 0) throw std::runtime_error("vertices size not multiple of stride");
//     if (mesh_data.indices.size() % 3 != 0) throw std::runtime_error("indices size not multiple of 3");
//     if (mesh_data.indices.empty()) { set_roi(glm::ivec3(0), glm::uvec3(0)); return; }

//     int pos_attr = vertex_layout.find_attribute_id_by_name("position");
//     const size_t pos_off_f = vertex_layout.attributes[pos_attr].offset / sizeof(float);
//     if (pos_off_f + 2 >= stride_f) throw std::runtime_error("position offset out of stride");

//     const size_t vertex_count = mesh_data.vertices.size() / stride_f;

//     glm::vec3 mn(std::numeric_limits<float>::infinity());
//     glm::vec3 mx(-std::numeric_limits<float>::infinity());

//     auto read_pos = [&](uint32_t vid) -> glm::vec3 {
//         if (vid >= vertex_count) throw std::runtime_error("index out of range");
//         size_t base = size_t(vid) * stride_f + pos_off_f;
//         glm::vec4 p(mesh_data.vertices[base + 0],
//                     mesh_data.vertices[base + 1],
//                     mesh_data.vertices[base + 2],
//                     1.0f);
//         glm::vec3 pv = glm::vec3(transform * p) / voxel_size;
//         return pv;
//     };

//     // IMPORTANT: считаем AABB только по реально используемым вершинам
//     for (uint32_t idx : mesh_data.indices) {
//         glm::vec3 pv = read_pos(idx);
//         // на всякий (если где-то NaN)
//         if (!finite3(pv)) continue;
//         mn = glm::min(mn, pv);
//         mx = glm::max(mx, pv);
//     }

//     if (!finite3(mn) || !finite3(mx)) {
//         set_roi(glm::ivec3(0), glm::uvec3(0));
//         return;
//     }

//     // делаем так же, как в шейдерах count/fill
//     const float eps = 1e-4f;
//     mn -= glm::vec3(eps);
//     mx += glm::vec3(eps);

//     glm::ivec3 vmin = glm::ivec3(glm::floor(mn));
//     glm::ivec3 vmax = glm::ivec3(glm::floor(mx));
//     vmax = glm::max(vmax, vmin);

//     if (pad_voxels > 0) {
//         vmin -= glm::ivec3(pad_voxels);
//         vmax += glm::ivec3(pad_voxels);
//     }

//     glm::ivec3 cmin(
//         math_utils::floor_div(vmin.x, chunk_size),
//         math_utils::floor_div(vmin.y, chunk_size),
//         math_utils::floor_div(vmin.z, chunk_size)
//     );
//     glm::ivec3 cmax(
//         math_utils::floor_div(vmax.x, chunk_size),
//         math_utils::floor_div(vmax.y, chunk_size),
//         math_utils::floor_div(vmax.z, chunk_size)
//     );

//     glm::ivec3 dim_i = cmax - cmin + glm::ivec3(1);
//     dim_i = glm::max(dim_i, glm::ivec3(0));

//     set_roi(cmin, glm::uvec3(dim_i));
// }



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

    // Важно: чтобы записи в SSBO стали видимы для readback/следующих проходов
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
    // calculate_roi_on_cpu(mesh_data, transform, vertex_layout, voxel_size, chunk_size, /*pad_voxels=*/1);
    set_roi(glm::ivec3(-10), glm::ivec3(20)); // ВРЕМЕННО!!! НЕ ЗАБЫТЬ ВЕРНУТЬ СТРОКУ ВЫШЕ!!!

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
