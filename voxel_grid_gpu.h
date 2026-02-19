#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>

#include "ssbo.h"
#include "shader_manager.h"
#include "compute_program.h"
#include "vf_program.h"
#include "math_utils.h"
#include "transformable.h"
#include "drawable.h"
#include "vao.h"
#include "vbo.h"
#include "ebo.h"

#define SLOT_EMPTY 0xFFFFFFFF
#define SLOT_LOCKED 0xFFFFFFFE
#define DONT_CHANGE 0xFFFFFFFF

class VoxelGridGPU : public Transformable, public Drawable {
public:
    glm::ivec3 chunk_size;
    uint32_t count_active_chunks;
    glm::vec3 voxel_size;
    uint32_t chunk_hash_table_size;
    uint32_t max_count_probing;
    uint32_t count_evict_buckets;
    uint32_t min_free_chunks;
    uint32_t max_evict_chunks;
    uint32_t bucket_step;

    const uint32_t min_free_pages = 256;

    struct ChunkMetaGPU {
        uint32_t used;
        uint32_t key_lo;
        uint32_t key_hi;
        uint32_t dirty_flags;
    };
    static_assert(sizeof(ChunkMetaGPU) == 16);

    struct alignas(8) VoxelDataGPU {
        uint32_t type_vis_flags;
        uint32_t color;
    };
    static_assert(sizeof(VoxelDataGPU) == 8);
    static_assert(alignof(VoxelDataGPU) == 8);

    struct alignas(16) VoxelWriteGPU {
        glm::ivec4 world_voxel;  // xyz, w unused
        VoxelDataGPU voxel_data;
        uint32_t pad0;
        uint32_t pad1;
    };
    static_assert(sizeof(VoxelWriteGPU) == 32);
    static_assert(alignof(VoxelWriteGPU) == 16);

    struct DrawElementsIndirectCommand {
        uint32_t count;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t  baseVertex;
        uint32_t baseInstance;
    };
    static_assert(sizeof(DrawElementsIndirectCommand) == 20);

    struct alignas(16) VertexGPU {
        glm::vec4 pos;    // xyz position, w=1
        uint32_t color;  // RGBA8
        uint32_t face;   // 0..5
        uint32_t pad0;
        uint32_t pad1;
    };
    static_assert(sizeof(VertexGPU) == 32);
    static_assert(alignof(VertexGPU) == 16);

    struct ChunkMeshAlloc {
        uint32_t v_startPage; 
        uint32_t v_order; 
        uint32_t needV; 
        uint32_t i_startPage; 
        uint32_t i_order; 
        uint32_t needI;
        uint32_t need_rebuild;
    };

    VoxelGridGPU(
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
        ShaderManager& shader_manager);

    void apply_writes_to_world_gpu(uint32_t write_count);
    void apply_writes_to_world_from_cpu(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels);

    void apply_writes_to_world_gpu_with_evict(uint32_t write_count, const glm::vec3& cam_pos);
    void apply_writes_to_world_from_cpu_with_evict(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels, 
                                                    const glm::vec3& cam_pos);
    void mark_chunk_to_generate(const glm::vec3& cam_world_pos, int radius_chunks);
    void generate_terrain(uint32_t seed, uint32_t load_count);
    void reset_load_list_counter();
    void stream_chunks_sphere(const glm::vec3& cam_world_pos, int radius_chunks, uint32_t seed);
    

    virtual void draw(RenderState state) override;

    //debug
    void print_counters(uint32_t write_count, uint32_t dirty_count, uint32_t cmd_count, uint32_t free_count, uint32_t load_list_count);
    void print_count_free_mesh_alloc();

    ComputeProgram prog_clear_chunks_;
    ComputeProgram prog_set_chunks_;
    ComputeProgram prog_world_init_;
    ComputeProgram prog_apply_writes_;
    ComputeProgram prog_mesh_counters_reset_;
    ComputeProgram prog_mesh_reset_;
    ComputeProgram prog_mesh_count_;
    ComputeProgram prog_mesh_alloc_;
    ComputeProgram prog_mesh_emit_;
    ComputeProgram prog_mesh_finalize_;
    ComputeProgram prog_cmdcount_reset_;
    ComputeProgram prog_build_indirect_;
    ComputeProgram prog_reset_dirty_count_;
    ComputeProgram prog_bucket_reset_;
    ComputeProgram prog_bucket_build_;
    ComputeProgram prog_evict_lowprio_;
    ComputeProgram prog_stream_select_chunks_;
    ComputeProgram prog_stream_generate_terrain_;
    ComputeProgram prog_mark_all_user_chunks_as_dirty_;
    ComputeProgram prog_mesh_pool_clear_;
    ComputeProgram prog_mesh_pool_seed_;
    ComputeProgram prog_reset_load_list_counter_;
    ComputeProgram prog_verify_mesh_allocation_;
    VfProgram prog_vf_voxel_mesh_diffusion_spec_;

    SSBO voxels_;
    SSBO chunk_meta_;
    SSBO chunk_hash_keys_;
    SSBO chunk_hash_vals_;
    SSBO free_list_;
    SSBO active_list_;
    SSBO frame_counters_;
    SSBO voxel_write_list_;
    SSBO equeued_;
    SSBO dirty_list_;
    SSBO global_vertex_buffer_;
    SSBO global_index_buffer_;
    SSBO chunk_indices_to_clear_;
    SSBO voxel_prifab_;
    SSBO chunk_indices_to_set_;
    SSBO coord_keys_to_set_;
    SSBO dirty_quad_count_;
    SSBO emit_counter_;
    SSBO mesh_counters_;
    SSBO indirect_cmds_;
    SSBO bucket_heads_;
    SSBO bucket_next_;
    SSBO stream_counters_;
    SSBO load_list_;
    SSBO failed_dirty_list_;

    SSBO vb_heads_;
    SSBO vb_next_;
    SSBO vb_state_;

    SSBO ib_heads_;
    SSBO ib_next_;
    SSBO ib_state_;
    
    SSBO alloc_markers_;

    SSBO chunk_mesh_alloc_local_;
    SSBO chunk_mesh_alloc_;
    SSBO count_free_pages_;

    SSBO debug_counters_;
    SSBO stream_generate_debug_counters_;
    
    size_t chunk_indices_to_clear_cap_bytes_ = 0;
    size_t chunk_indices_to_set_cap_bytes_ = 0;
    size_t coord_keys_to_set_cap_bytes_ = 0;
    size_t voxel_write_list_cap_bytes_ = 0;

    uint32_t vb_page_size_ = 0;
    uint32_t count_vb_pages_ = 0;
    uint32_t vb_index_bits_ = 0;
    uint32_t vb_order_ = 0;
    uint32_t max_mesh_vertices_ = 0;
    
    uint32_t ib_page_size_ = 0;
    uint32_t count_ib_pages_ = 0;
    uint32_t ib_index_bits_ = 0;
    uint32_t ib_order_ = 0;
    uint32_t max_mesh_indices_ = 0;

    VBO vbo;
    EBO ebo;
    VAO vao;

    void init_programs(ShaderManager& shader_manager);

    void set_frame_counters(
        uint32_t write_count = DONT_CHANGE, 
        uint32_t dirty_count = DONT_CHANGE, 
        uint32_t cmd_count = DONT_CHANGE, 
        uint32_t free_count = DONT_CHANGE
    );

    uint32_t read_dirty_count_cpu();


    void world_init_gpu();

    void init_mesh_pool();

    void clear_chunks(std::vector<uint32_t>& chunk_ids, const VoxelDataGPU& init_voxel_prifab);
    void init_active_chunks(glm::ivec3 chunk_size, uint32_t count_active_chunks, const VoxelDataGPU& init_voxel_prifab);

    void init_chunks_hash_table();
    void set_chunks(const std::vector<uint32_t>& chunk_ids, const std::vector<glm::ivec3>& chunk_coords, bool set_with_replace);
    void set_chunks(const std::vector<uint32_t>& chunk_ids, const std::vector<uint64_t>& coord_keys, bool set_with_replace);

    void ensure_set_chunk_buffers(const std::vector<uint32_t>& chunk_ids, const std::vector<uint64_t>& coord_keys);
    void ensure_free_chunks_gpu(const glm::vec3& camPos);
    void ensure_voxel_write_list(size_t count);

    void build_mesh_from_dirty(uint32_t pack_bits, int pack_offset);
    void build_indirect_draw_commands_frustum(const glm::mat4& viewProj, uint32_t pack_bits, int pack_offset);
    void draw_indirect(const GLuint vao, const glm::mat4& world, const glm::mat4& proj_view, const glm::vec3& cam_pos);

    void init_draw_buffers();

    void mark_all_used_chunks_as_dirty(); // Говно медленное
};