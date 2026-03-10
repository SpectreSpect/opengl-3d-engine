#pragma once
#define NOMINMAX
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
#include "path_utils.h"
#include <unordered_set>
#include <fstream>
#include <set>
#include <algorithm>
#include <limits>

#define DONT_CHANGE 0xFFFFFFFF

class VoxelGridGPU : public Transformable, public Drawable {
public:
    static constexpr uint32_t INVALID_ID = 0xFFFFFFFFu;

    static constexpr uint32_t ST_MASK_BITS = 4u;
    static constexpr uint32_t ST_MASK = (1u << ST_MASK_BITS) - 1u;
    
    static constexpr uint32_t ST_FREE = 0u;
    static constexpr uint32_t ST_ALLOC = 1u;
    static constexpr uint32_t ST_MERGED = 2u;
    
    static constexpr uint32_t HEAD_TAG_BITS = 4u;
    static constexpr uint32_t HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
    static constexpr uint32_t INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;

    static constexpr uint32_t USE_DIRECT_VALUE = 0xFFFFFFFFu;

    static constexpr uint32_t SLOT_EMPTY = 0xFFFFFFFFu;
    static constexpr uint32_t SLOT_LOCKED = 0xFFFFFFFEu;
    static constexpr uint32_t SLOT_TOMB = 0xFFFFFFFDu; 

    glm::ivec3 chunk_size;
    uint32_t count_active_chunks;
    glm::vec3 voxel_size;
    uint32_t chunk_hash_table_size;
    uint32_t max_count_probing;
    uint32_t count_evict_buckets;
    uint32_t min_free_chunks;
    uint32_t max_evict_chunks;
    uint32_t eviction_bucket_shell_thickness;

    const uint32_t min_free_pages = 1024;

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


    struct PushLoopData {
        uint32_t old_h;
        uint32_t next_val;
        uint32_t old_value_of_next_val;
        uint32_t new_tag;
        uint32_t new_head;
        uint32_t was_head;
    };

    struct PushData {
        uint32_t input_start_page;
        uint32_t input_order;
        uint32_t old_state_of_input_start_page;
        uint32_t count_loop_cycles;
        PushLoopData loop_data[32];
    };

    struct MergeData {
        uint32_t cur_start;
        uint32_t cur_order;
        uint32_t cur_state;
        uint32_t buddy_size;
        uint32_t start_buddy;
        uint32_t buddy_state;
    };

    struct DebugStackElement {
        uint32_t dirty_idx;
        uint32_t chunk_id;
        uint32_t a_i_startPage; 
        uint32_t i_order;
        uint32_t prev_alloc_state;
        uint32_t count_merges;
        MergeData merge_data[33];
        uint32_t result_start;
        uint32_t result_order;
        uint32_t state_before_change;
        uint32_t state_after_change;
        PushData push_data;
        uint32_t current_head;
        uint32_t bool_push_result;
    };

    struct AllocNode {
        uint32_t page;
        uint32_t next;
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
        float eviction_bucket_shell_thickness,
        uint32_t vb_page_size_order_of_two,
        uint32_t ib_page_size_order_of_two,
        float buddy_allocator_nodes_factor,
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

    ShaderManager* shader_manager = nullptr;

    ComputeProgram prog_dispatch_adapter_;
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
    ComputeProgram prog_evict_low_priority_dispatch_adapter_;
    ComputeProgram prog_stream_select_chunks_;
    ComputeProgram prog_stream_generate_terrain_;
    ComputeProgram prog_mark_all_user_chunks_as_dirty_;
    ComputeProgram prog_mesh_pool_clear_;
    ComputeProgram prog_mesh_pool_seed_;
    ComputeProgram prog_reset_load_list_counter_;
    ComputeProgram prog_verify_mesh_allocation_;
    ComputeProgram prog_return_free_alloc_nodes_;
    ComputeProgram prog_return_free_alloc_nodes_dispatch_adapter_;
    ComputeProgram prog_free_evicted_chunks_mesh_;
    ComputeProgram prog_fill_chunk_hash_table_;
    ComputeProgram prog_clear_chunk_hash_table_;
    VfProgram prog_vf_voxel_mesh_diffusion_spec_;

    SSBO dispatch_args;
    SSBO voxels_;
    SSBO chunk_meta_;
    SSBO chunk_hash_keys_;
    SSBO chunk_hash_vals_;
    SSBO free_list_;
    SSBO active_list_;
    SSBO frame_counters_;
    SSBO voxel_write_list_;
    SSBO enqueued_;
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
    SSBO verify_debug_stack_;
    SSBO evicted_chunks_list_;

    SSBO vb_heads_;
    SSBO vb_state_;
    SSBO vb_nodes_;
    SSBO vb_free_nodes_list_;
    SSBO vb_returned_nodes_list;

    SSBO ib_heads_;
    SSBO ib_state_;
    SSBO ib_nodes_;
    SSBO ib_free_nodes_list_;
    SSBO ib_returned_nodes_list;
    
    SSBO alloc_markers_;

    SSBO chunk_mesh_alloc_local_;
    SSBO chunk_mesh_alloc_;
    SSBO count_free_pages_;

    SSBO debug_counters_vb_;
    SSBO debug_counters_ib_;
    SSBO stream_generate_debug_counters_;

    SSBO vb_alloc_stack_;
    SSBO ib_alloc_stack_;
    
    size_t chunk_indices_to_clear_cap_bytes_ = 0;
    size_t chunk_indices_to_set_cap_bytes_ = 0;
    size_t coord_keys_to_set_cap_bytes_ = 0;
    size_t voxel_write_list_cap_bytes_ = 0;

    uint32_t vb_page_size_ = 0;
    uint32_t count_vb_pages_ = 0;
    uint32_t count_vb_nodes_ = 0;
    uint32_t vb_index_bits_ = 0;
    uint32_t vb_order_ = 0;
    uint32_t max_mesh_vertices_ = 0;
    
    uint32_t ib_page_size_ = 0;
    uint32_t count_ib_pages_ = 0;
    uint32_t count_ib_nodes_ = 0;
    uint32_t ib_index_bits_ = 0;
    uint32_t ib_order_ = 0;
    uint32_t max_mesh_indices_ = 0;

    VBO vbo;
    EBO ebo;
    VAO vao;

    // void load_alloc_stacks();

    void init_programs(ShaderManager& shader_manager);

    void set_frame_counters(
        uint32_t write_count = DONT_CHANGE, 
        uint32_t dirty_count = DONT_CHANGE, 
        uint32_t cmd_count = DONT_CHANGE, 
        uint32_t free_count = DONT_CHANGE
    );

    uint32_t read_dirty_count_cpu();


    void world_init_gpu(); // ++++++++++++++++++++

    void init_mesh_pool(); // ++++++++++++++++++++

    // void clear_chunks(std::vector<uint32_t>& chunk_ids, const VoxelDataGPU& init_voxel_prifab);
    // void init_active_chunks(glm::ivec3 chunk_size, uint32_t count_active_chunks, const VoxelDataGPU& init_voxel_prifab);

    // void init_chunks_hash_table();
    // void set_chunks(const std::vector<uint32_t>& chunk_ids, const std::vector<glm::ivec3>& chunk_coords, bool set_with_replace);
    // void set_chunks(const std::vector<uint32_t>& chunk_ids, const std::vector<uint64_t>& coord_keys, bool set_with_replace);
    void print_chunks_hash_table_log(); // ++++++++++++++++++++

    void clear_chunk_hash_table(); // ++++++++++++++++++++
    void fill_chunk_hash_table(uint32_t pack_bits, uint32_t pack_offset); // ++++++++++++++++++++
    void rebuild_chunk_hash_table(uint32_t pack_bits, uint32_t pack_offset); // ++++++++++++++++++++

    void prepare_dispatch_args(
        SSBO& dispatch_args,
        const SSBO* arg_buffer_0, 
        const SSBO* arg_buffer_1, 
        const SSBO* arg_buffer_2, 
        uint32_t offset_0 = USE_DIRECT_VALUE,
        uint32_t offset_1 = USE_DIRECT_VALUE,
        uint32_t offset_2 = USE_DIRECT_VALUE,
        uint32_t direct_value_0 = 1u,
        uint32_t direct_value_1 = 1u,
        uint32_t direct_value_2 = 1u,
        uint32_t x_workgroup_size = 256u,
        uint32_t y_workgroup_size = 1u,
        uint32_t z_workgroup_size = 1u
    ); // ++++++++++++++++++++

    // void ensure_set_chunk_buffers(const std::vector<uint32_t>& chunk_ids, const std::vector<uint64_t>& coord_keys);
    void print_eviction_log(const glm::vec3& camera_pos); // ++++++++++++++++++++

    void reset_heads(); // ++++++++++++++++++++
    void build_bucket_lists(const glm::vec3& cam_pos); // ++++++++++++++++++++
    void prepare_evict_lowpriority_chunks(const SSBO& dispatch_args); // ++++++++++++++++++++
    void evict_lowpriority_chunks(const SSBO& dispatch_args); // ++++++++++++++++++++
    void free_evicted_chunks_mesh(const SSBO& dispatch_args); // ++++++++++++++++++++
    void ensure_free_chunks_gpu(const glm::vec3& cam_pos, uint32_t pack_bits, uint32_t pack_offset); // ++++++++++++++++++++
    void ensure_voxel_write_list(size_t count); // ++++++++++++++++++++

    void reset_global_mesh_counters(); // ++++++++++++++++++++
    void mesh_reset(const SSBO& dispatch_args); // ++++++++++++++++++++
    void mesh_count(const SSBO& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); // ++++++++++++++++++++
    void mesh_alloc_vb(const SSBO& dispatch_args); // ++++++++++++++++++++
    void mesh_alloc_ib(const SSBO& dispatch_args); // ++++++++++++++++++++
    void mesh_alloc(const SSBO& dispatch_args); // ++++++++++++++++++++
    void verify_mesh_allocation(const SSBO& dispatch_args); // ++++++++++++++++++++
    void prepare_return_free_alloc_nodes(SSBO& dispatch_args); // ++++++++++++++++++++
    void return_free_alloc_nodes(const SSBO& dispatch_args); // ++++++++++++++++++++
    void mesh_emit(const SSBO& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); // ++++++++++++++++++++
    void mesh_finalize(const SSBO& dispatch_args); // ++++++++++++++++++++
    void reset_dirty_count(); // ++++++++++++++++++++
    void build_mesh_from_dirty(uint32_t pack_bits, int pack_offset); // ++++++++++++++++++++

    // void reset_cmd_count();
    void build_draw_commands(const glm::mat4& view_proj, uint32_t pack_bits, int pack_offset); // ++++++++++++++++++++
    void build_indirect_draw_commands_frustum(const glm::mat4& viewProj, uint32_t pack_bits, int pack_offset); // ++++++++++++++++++++

    void draw_indirect(const GLuint vao, const glm::mat4& world, const glm::mat4& proj_view, const glm::vec3& cam_pos); // ++++++++++++++++++++

    void init_draw_buffers(); // ++++++++++++++++++++

    void mark_all_used_chunks_as_dirty(); // Говно медленное // ++++++++++++++++++++

    void read_states_data(
        std::unordered_map<uint32_t, uint32_t>& meta_alloc_vb_out, 
        std::unordered_map<uint32_t, uint32_t>& meta_alloc_ib_out,
        std::unordered_map<uint32_t, uint32_t>& states_alloc_vb_out, 
        std::unordered_map<uint32_t, uint32_t>& states_alloc_ib_out
    ); // ++++++++++++++++++++

    void save_verify_mesh_buffers_dumps(std::filesystem::path dir); // ++++++++++++++++++++
    void load_verify_mesh_buffers_dumps(std::filesystem::path dir); // ++++++++++++++++++++
    
    std::set<uint32_t> find_limbo_pages(SSBO& heads_buffer, SSBO& states_buffer, SSBO& next_buffer, uint32_t max_order_in_heads_buffer, uint32_t count_pages_in_states_buffer); // ++++++++++++++++++++
    void print_verify_debug_stack(uint32_t offset, int count_elements_to_print = -1); // ++++++++++++++++++++
};