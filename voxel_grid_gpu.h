#pragma once
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <unordered_set>
#include <fstream>
#include <set>
#include <algorithm>
#include <limits>

#include "buffer_object.h"
#include "shader_manager.h"
#include "compute_program.h"
#include "vf_program.h"
#include "math_utils.h"
#include "transformable.h"
#include "drawable.h"
#include "vao.h"
#include "path_utils.h"
#include "buffer_dispatch_arg.h"
#include "value_dispatch_arg.h"

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

    static constexpr uint32_t SLOT_EMPTY = 0xFFFFFFFFu;
    static constexpr uint32_t SLOT_LOCKED = 0xFFFFFFFEu;
    static constexpr uint32_t SLOT_TOMB = 0xFFFFFFFDu; 

    glm::ivec3 chunk_size;
    uint32_t count_active_chunks;
    glm::vec3 voxel_size;
    uint32_t chunk_hash_table_size;
    uint32_t count_evict_buckets;
    uint32_t min_free_chunks;
    float tomb_fraction_to_rebuild;
    uint32_t eviction_bucket_shell_thickness;
    uint32_t vox_per_chunk;
    float render_distance;

    const uint32_t min_free_pages = 1024;

    struct BucketHead {
        uint32_t id;
        uint32_t count;
    };

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

        VoxelDataGPU() = default;

        inline VoxelDataGPU(uint32_t type, uint32_t visability, uint32_t flags, uint32_t color) {
            init(type, visability, flags, color);
        }

        inline VoxelDataGPU(uint32_t type, uint32_t visability, uint32_t flags, glm::ivec4 color) {
            uint32_t packed_color = ((color.x & 0xFFu) << 24u) | ((color.y & 0xFFu) << 16u) | ((color.z & 0xFFu) << 8u) | (color.w & 0xFFu);
            init(type, visability, flags, packed_color);
        }

        inline VoxelDataGPU(uint32_t type, uint32_t visability, uint32_t flags, glm::ivec3 color) {
            uint32_t packed_color = ((color.x & 0xFFu) << 24u) | ((color.y & 0xFFu) << 16u) | ((color.z & 0xFFu) << 8u) | 0xFFu;
            init(type, visability, flags, packed_color);
        }

        inline void init(uint32_t type, uint32_t visability, uint32_t flags, uint32_t color) {
            this->type_vis_flags = ((type & 0xFFu) << 16) | ((visability & 0xFFu) << 8) | (flags & 0xFFu); // Тип 0
            this->color = color;
        }
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
        uint32_t count_evict_buckets,
        uint32_t min_free_chunks,
        float tomb_fraction_to_rebuild,
        float eviction_bucket_shell_thickness,
        uint32_t vb_page_size_order_of_two,
        uint32_t ib_page_size_order_of_two,
        float buddy_allocator_nodes_factor,
        float render_distance,
        ShaderManager& shader_manager);

    void apply_writes_to_world_gpu(uint32_t write_count);
    void apply_writes_to_world_from_cpu(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels);

    void apply_writes_to_world_gpu_with_evict(uint32_t write_count, const glm::vec3& cam_pos);
    void apply_writes_to_world_from_cpu_with_evict(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels, 
                                                    const glm::vec3& cam_pos);
    void reset_load_list_counter();
    void mark_chunk_to_generate(const glm::vec3& cam_world_pos, int radius_chunks);
    void generate_terrain(const BufferObject& dispatch_args, uint32_t seed);
    void stream_chunks_sphere(const glm::vec3& cam_world_pos, int radius_chunks, uint32_t seed);
    
    virtual void draw(RenderState state) override;


    ShaderManager* shader_manager = nullptr;

    ComputeProgram prog_dispatch_adapter_;
    ComputeProgram prog_clear_chunks_;
    ComputeProgram prog_world_init_;
    ComputeProgram prog_apply_writes_;
    ComputeProgram prog_mesh_reset_;
    ComputeProgram prog_mesh_count_;
    ComputeProgram prog_mesh_alloc_;
    ComputeProgram prog_mesh_emit_;
    ComputeProgram prog_mesh_finalize_;
    ComputeProgram prog_cmdcount_reset_;
    ComputeProgram prog_build_indirect_cmds_;
    ComputeProgram prog_reset_dirty_count_;
    ComputeProgram prog_evict_buckets_build_;
    ComputeProgram prog_evict_low_priority_;
    ComputeProgram prog_evict_low_priority_dispatch_adapter_;
    ComputeProgram prog_stream_select_chunks_;
    ComputeProgram prog_stream_generate_terrain_;
    ComputeProgram prog_mark_all_user_chunks_as_dirty_;
    ComputeProgram prog_mesh_pool_clear_;
    ComputeProgram prog_mesh_pool_seed_;
    ComputeProgram prog_verify_mesh_allocation_;
    ComputeProgram prog_return_free_alloc_nodes_;
    ComputeProgram prog_return_free_alloc_nodes_dispatch_adapter_;
    ComputeProgram prog_free_evicted_chunks_mesh_;
    ComputeProgram prog_fill_chunk_hash_table_;
    ComputeProgram prog_clear_chunk_hash_table_;
    ComputeProgram prog_reset_evicted_list_and_buckets_;
    ComputeProgram prog_hash_table_conditional_dispatch_adapter_;
    VfProgram prog_vf_voxel_mesh_diffusion_spec_;

    BufferObject dispatch_args;
    BufferObject dispatch_args_additional;
    BufferObject voxels_;
    BufferObject chunk_meta_;
    BufferObject chunk_hash_keys_;
    BufferObject chunk_hash_vals_;
    BufferObject free_list_;
    BufferObject mesh_buffers_status_;
    BufferObject voxel_write_list_;
    BufferObject enqueued_;
    BufferObject dirty_list_;
    BufferObject global_vertex_buffer_;
    BufferObject global_index_buffer_;
    BufferObject voxel_prifab_;
    BufferObject dirty_quad_count_;
    BufferObject emit_counters_;
    BufferObject indirect_cmds_;
    BufferObject bucket_heads_;
    BufferObject bucket_next_;
    BufferObject load_list_;
    BufferObject failed_dirty_list_;
    BufferObject verify_debug_stack_;
    BufferObject evicted_chunks_list_;

    BufferObject vb_heads_;
    BufferObject vb_state_;
    BufferObject vb_nodes_;
    BufferObject vb_free_nodes_list_;
    BufferObject vb_returned_nodes_list;

    BufferObject ib_heads_;
    BufferObject ib_state_;
    BufferObject ib_nodes_;
    BufferObject ib_free_nodes_list_;
    BufferObject ib_returned_nodes_list;

    BufferObject chunk_mesh_alloc_local_;
    BufferObject chunk_mesh_alloc_;

    uint32_t vb_page_size_ = 0;
    uint32_t count_vb_pages_ = 0;
    uint32_t count_vb_nodes_ = 0;
    uint32_t vb_order_ = 0;
    uint32_t max_mesh_vertices_ = 0;
    
    uint32_t ib_page_size_ = 0;
    uint32_t count_ib_pages_ = 0;
    uint32_t count_ib_nodes_ = 0;
    uint32_t ib_order_ = 0;
    uint32_t max_mesh_indices_ = 0;

    VAO vao;

    void init_programs(ShaderManager& shader_manager);
    void world_init_gpu();
    void init_mesh_pool();

    void clear_chunk_hash_table(const BufferObject& dispatch_args); 
    void fill_chunk_hash_table(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); 
    void conditional_prepare_rebuild(BufferObject& clear_dispatch_args, BufferObject& fill_dispatch_args);
    void rebuild_chunk_hash_table(uint32_t pack_bits, uint32_t pack_offset); 

    void prepare_dispatch_args(
        BufferObject& dispatch_args,
        const DispatchArg& arg_x = ValueDispatchArg(1u),
        const DispatchArg& arg_y = ValueDispatchArg(1u),
        const DispatchArg& arg_z = ValueDispatchArg(1u)
    );

    void reset_heads(); 
    void build_bucket_lists(const glm::vec3& cam_pos); 
    void prepare_evict_lowpriority_chunks(const BufferObject& dispatch_args); 
    void evict_lowpriority_chunks(const BufferObject& dispatch_args); 
    void free_evicted_chunks_mesh(const BufferObject& dispatch_args); 
    void reset_evicted_list_and_buckets();
    void ensure_free_chunks_gpu(const glm::vec3& cam_pos, uint32_t pack_bits, uint32_t pack_offset); 
    void ensure_voxel_write_list(size_t count); 

    void mesh_reset(const BufferObject& dispatch_args); 
    void mesh_count(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); 
    void mesh_alloc_vb(const BufferObject& dispatch_args); 
    void mesh_alloc_ib(const BufferObject& dispatch_args); 
    void mesh_alloc(const BufferObject& dispatch_args); 
    void verify_mesh_allocation(const BufferObject& dispatch_args); 
    void prepare_return_free_alloc_nodes(BufferObject& dispatch_args); 
    void return_free_alloc_nodes(const BufferObject& dispatch_args); 
    void mesh_emit(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); 
    void mesh_finalize(const BufferObject& dispatch_args); 
    void reset_dirty_count(); 
    void build_mesh_from_dirty(uint32_t pack_bits, int pack_offset); 

    void reset_cmd_count();
    void build_draw_commands(const glm::mat4& view_proj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset); 
    void build_indirect_draw_commands_frustum(const glm::mat4& viewProj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset); 

    void draw_indirect(const GLuint vao, const glm::mat4& world, const glm::mat4& proj_view, const glm::vec3& cam_pos); 

    void init_draw_buffers(); 

    void mark_all_used_chunks_as_dirty(); // Говно медленное 
};