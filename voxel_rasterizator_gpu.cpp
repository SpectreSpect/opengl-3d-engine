#include "voxel_rasterizator_gpu.h"

VoxelRasterizatorGPU::VoxelRasterizatorGPU(const VoxelRasterizatorDesc& desc, IGridable* gridable, ShaderManager* shader_manager, ShaderHelper* shader_helper) 
    :   chunk_size(desc.chunk_size),
        voxel_size(desc.voxel_size),
        counter_hash_table_size(desc.counter_hash_table_size),
        count_voxel_writes(desc.count_voxel_writes),
        gridable(gridable),
        gridable_gpu(dynamic_cast<IGridableGPU*>(gridable)),
        shader_manager(shader_manager),
        shader_helper(shader_helper)
{
    dispatch_args = BufferObject(sizeof(uint32_t) * 3, GL_DYNAMIC_DRAW);
    counter_hash_table_ = BufferObject(sizeof(HashTableCounters) + sizeof(CounterHashTableSlot) * desc.counter_hash_table_size, GL_DYNAMIC_DRAW);
    active_chunk_keys_list_ = BufferObject(sizeof(uint64_t) * (desc.counter_hash_table_size + 1u), GL_DYNAMIC_DRAW);
    triangle_indices_list_ = BufferObject(sizeof(uint32_t) * (desc.counter_hash_table_size + 1u), GL_DYNAMIC_DRAW);
    voxel_writes_ = BufferObject(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * count_voxel_writes, GL_DYNAMIC_DRAW);

    init_programs();
}

VoxelRasterizatorGPU::~VoxelRasterizatorGPU() {

}

void VoxelRasterizatorGPU::init_programs() {
    // prog_count_ = ComputeProgram(&shader_manager->count_triangles_in_chunks_cs);
    // prog_scan_blocks_ = ComputeProgram(&shader_manager->scan_blocks_cs);
    // prog_add_block_offsets_ = ComputeProgram(&shader_manager->add_block_offsets_cs);
    // prog_fix_last_ = ComputeProgram(&shader_manager->fix_last_cs);
    // prog_copy_offsets_to_cursor_ = ComputeProgram(&shader_manager->copy_offsets_to_cursor_cs);
    // prog_fill_triangle_indices_ = ComputeProgram(&shader_manager->fill_triangle_indices_cs);

    // prog_roi_reduce_indices_ = ComputeProgram(&shader_manager->roi_reduce_indices_cs);
    // prog_roi_reduce_pairs_ = ComputeProgram(&shader_manager->roi_reduce_pairs_cs);
    // prog_roi_finalize_ = ComputeProgram(&shader_manager->roi_finalize_cs);
    // prog_build_active_chunks_ = ComputeProgram(&shader_manager->build_active_chunks_cs);
    // prog_build_voxel_writes_ = ComputeProgram(&shader_manager->build_voxel_writes_cs);

    prog_reset_voxelize_pipeline_ = ComputeProgram(&shader_manager->reset_voxelize_pipeline_cs);
    prog_mark_and_count_active_chunks_ = ComputeProgram(&shader_manager->mark_and_count_active_chunks_cs);
    prog_alloc_active_chunk_triangles_ = ComputeProgram(&shader_manager->alloc_active_chunk_triangles_cs);
    // prog_copy_counters_from_counter_hash_table_ = ComputeProgram(&shader_manager->copy_counters_from_counter_hash_table_cs);
    prog_voxelize_triangles_ = ComputeProgram(&shader_manager->voxelize_triangles_cs);
}

void VoxelRasterizatorGPU::reset_voxelize_pipline(BufferObject& voxel_writes, bool reset_voxel_write_list) {
    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    triangle_indices_list_.bind_base_as_ssbo(2);
    voxel_writes.bind_base_as_ssbo(3);

    prog_reset_voxelize_pipeline_.use();

    glUniform1ui(glGetUniformLocation(prog_reset_voxelize_pipeline_.id, "u_counter_hash_table_size"), counter_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_reset_voxelize_pipeline_.id, "u_reset_voxel_write_list"), reset_voxel_write_list ? 1u : 0u);

    uint32_t slot_groups = math_utils::div_up_u32(counter_hash_table_size, 256u);
    prog_reset_voxelize_pipeline_.dispatch_compute(slot_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::mark_and_count_active_chunks(const Mesh& mesh) {
    glm::mat4x4 transform = mesh.get_model_matrix();
    uint32_t count_triangles = mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3);

    size_t position_attribute_id = mesh.vertex_layout.find_attribute_id_by_name("position");
    uint32_t vertex_stride_bytes = mesh.vertex_layout.attributes[position_attribute_id].stride;
    uint32_t position_offset_bytes = mesh.vertex_layout.attributes[position_attribute_id].offset;

    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    mesh.vbo.bind_base_as_ssbo(2);
    mesh.ebo.bind_base_as_ssbo(3);

    prog_mark_and_count_active_chunks_.use();
    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_count_mesh_triangles"), count_triangles);

    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_counter_hash_table_size"), counter_hash_table_size);

    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_vertex_stride_bytes"), vertex_stride_bytes);
    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_vertex_position_offset_bytes"), position_offset_bytes);

    glUniform3f(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform3ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_chunk_size"), chunk_size.x, chunk_size.y, chunk_size.z);

    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_pack_bits"), math_utils::BITS);

    glUniformMatrix4fv(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_transform"), 1, GL_FALSE, &transform[0][0]);

    uint32_t count_triangle_groups = math_utils::div_up_u32(count_triangles, 256u);
    prog_mark_and_count_active_chunks_.dispatch_compute(count_triangle_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::alloc_active_chunk_triangles() {    
    counter_hash_table_.bind_base_as_ssbo(0);
    triangle_indices_list_.bind_base_as_ssbo(1);

    prog_alloc_active_chunk_triangles_.use();

    glUniform1ui(glGetUniformLocation(prog_alloc_active_chunk_triangles_.id, "u_counter_hash_table_size"), counter_hash_table_size);

    uint32_t slot_groups = math_utils::div_up_u32(counter_hash_table_size, 256u);
    prog_alloc_active_chunk_triangles_.dispatch_compute(slot_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::fill_triangle_indices(const Mesh& mesh) {
    glm::mat4x4 transform = mesh.get_model_matrix();
    uint32_t count_triangles = mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3);

    counter_hash_table_.bind_base_as_ssbo(0);
    triangle_indices_list_.bind_base_as_ssbo(1);
    mesh.vbo.bind_base_as_ssbo(2);
    mesh.ebo.bind_base_as_ssbo(3);

    prog_fill_triangle_indices_.use();

    size_t position_attribute_id = mesh.vertex_layout.find_attribute_id_by_name("position");
    uint32_t vertex_stride_bytes = mesh.vertex_layout.attributes[position_attribute_id].stride;
    uint32_t position_offset_bytes = mesh.vertex_layout.attributes[position_attribute_id].offset;

    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_count_mesh_triangles"), count_triangles);
    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_counter_hash_table_size"), counter_hash_table_size);

    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_vertex_stride_bytes"), vertex_stride_bytes);
    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_vertex_position_offset_bytes"), position_offset_bytes);

    glUniform3f(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform3ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_chunk_size"), chunk_size.x, chunk_size.y, chunk_size.z);

    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_pack_bits"), math_utils::BITS);
    
    glUniformMatrix4fv(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_transform"), 1, GL_FALSE, &transform[0][0]);

    uint32_t count_triangle_groups = math_utils::div_up_u32(count_triangles, 256u);
    prog_fill_triangle_indices_.dispatch_compute(count_triangle_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::voxelize_chunks(
    const BufferObject& dispatch_args,
    const Mesh& mesh,
    BufferObject& voxel_writes,
    uint32_t voxel_type_vis_flags,
    uint32_t voxel_color,
    uint32_t voxel_set_flags) 
{
    glm::mat4x4 transform = mesh.get_model_matrix();
    uint32_t count_triangles = mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3);

    size_t position_attribute_id = mesh.vertex_layout.find_attribute_id_by_name("position");
    uint32_t vertex_stride_bytes = mesh.vertex_layout.attributes[position_attribute_id].stride;
    uint32_t position_offset_bytes = mesh.vertex_layout.attributes[position_attribute_id].offset;

    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    triangle_indices_list_.bind_base_as_ssbo(2);
    mesh.vbo.bind_base_as_ssbo(3);
    mesh.ebo.bind_base_as_ssbo(4);
    voxel_writes_.bind_base_as_ssbo(5);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_voxelize_triangles_.use();

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_counter_hash_table_size"), counter_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_count_voxels_in_chunk"), chunk_size.x * chunk_size.y * chunk_size.z);

    glUniform3ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_voxelize_triangles_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniformMatrix4fv(glGetUniformLocation(prog_voxelize_triangles_.id, "u_transform"), 1, GL_FALSE, &transform[0][0]);

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_pack_bits"), math_utils::BITS);

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "voxel_type_vis_flags"), voxel_type_vis_flags);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "voxel_color"), voxel_color);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "voxel_set_flags"), voxel_set_flags);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::rasterize(const Mesh& mesh, const VoxelWriteGPU& prifab, BufferObject* out_voxel_writes)
{
    BufferObject* voxel_writes_buffer = out_voxel_writes ? out_voxel_writes : &voxel_writes_;

    reset_voxelize_pipline(*voxel_writes_buffer, out_voxel_writes == nullptr);
    mark_and_count_active_chunks(mesh);
    alloc_active_chunk_triangles();
    fill_triangle_indices(mesh);

    shader_helper->prepare_dispatch_args(
        dispatch_args, 
        ValueDispatchArg(chunk_size.x*chunk_size.y*chunk_size.z), 
        BufferDispatchArg(&active_chunk_keys_list_, 0)
    );
    voxelize_chunks(
        dispatch_args,
        mesh,
        *voxel_writes_buffer,
        prifab.voxel_data.type_vis_flags,
        prifab.voxel_data.color,
        prifab.set_flags
    );
    
    if (out_voxel_writes == nullptr) {
        if (gridable_gpu != nullptr) {
            gridable_gpu->set_voxels(voxel_writes_);
        } else {
            //TODO
        }
    }
}
