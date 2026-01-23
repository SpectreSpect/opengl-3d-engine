#include "voxel_editor.h"
#include "voxel_grid.h"

void VoxelEditor::update_and_schedule() {
    for (auto chunk_map_it = edited_voxels.begin(); chunk_map_it != edited_voxels.end();) {
        uint64_t chunk_key = chunk_map_it->first;
        glm::ivec3 chunk_pos = VoxelGrid::unpack_key(chunk_key);

        auto chunk_it = voxel_grid->chunks.find(chunk_key);
        if (chunk_it == voxel_grid->chunks.end()) {
            
            Chunk* new_chunk = new Chunk(voxel_grid->chunk_size, {1, 1, 1});
            new_chunk->position = glm::vec3(chunk_pos.x * voxel_grid->chunk_size.x, chunk_pos.y * voxel_grid->chunk_size.y, chunk_pos.z * voxel_grid->chunk_size.z);
            voxel_grid->chunks[chunk_key] = new_chunk;
        }
        
        Chunk* chunk_to_edit = this->voxel_grid->chunks[chunk_key];

        chunk_to_edit->edit_voxels([&](std::vector<Voxel>& voxels){
            auto voxel_map = chunk_map_it->second;

            for (auto voxel_it = voxel_map.begin(); voxel_it != voxel_map.end(); ++voxel_it) {
                uint64_t local_voxel_key = voxel_it->first;
                glm::ivec3 local_voxel_pos = VoxelGrid::unpack_key(local_voxel_key);

                int id = Chunk::idx(local_voxel_pos, voxel_grid->chunk_size);

                Voxel voxel = voxel_it->second;

                voxels[id] = voxel;
            }
        });

        voxel_grid->chunks_to_update.insert(chunk_key);
        voxel_grid->chunks_to_update.insert(VoxelGrid::pack_key(chunk_pos.x-1, chunk_pos.y, chunk_pos.z)); // left
        voxel_grid->chunks_to_update.insert(VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z-1)); // back
        voxel_grid->chunks_to_update.insert(VoxelGrid::pack_key(chunk_pos.x+1, chunk_pos.y, chunk_pos.z)); // right
        voxel_grid->chunks_to_update.insert(VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z+1)); // front
        voxel_grid->chunks_to_update.insert(VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y+1, chunk_pos.z)); // top
        voxel_grid->chunks_to_update.insert(VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y-1, chunk_pos.z)); // bottom

        chunk_map_it = edited_voxels.erase(chunk_map_it);
    }
}

void VoxelEditor::set(glm::ivec3 pos, const Voxel& voxel) {
    glm::ivec3 chunk_pos = VoxelGrid::get_chunk_pos(pos, voxel_grid->chunk_size);
    uint64_t chunk_key = VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);

    int lx = pos.x - chunk_pos.x * voxel_grid->chunk_size.x;
    int ly = pos.y - chunk_pos.y * voxel_grid->chunk_size.y;
    int lz = pos.z - chunk_pos.z * voxel_grid->chunk_size.z;

    glm::ivec3 local_voxel_pos = glm::ivec3(lx, ly, lz);
    uint64_t local_voxel_key = VoxelGrid::pack_key(lx, ly, lz);

    edited_voxels[chunk_key][local_voxel_key] = voxel;
}

Voxel VoxelEditor::get(glm::ivec3 pos) const{
    glm::ivec3 chunk_pos = VoxelGrid::get_chunk_pos(pos, voxel_grid->chunk_size);
    uint64_t chunk_key = VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);

    int lx = pos.x - chunk_pos.x * voxel_grid->chunk_size.x;
    int ly = pos.y - chunk_pos.y * voxel_grid->chunk_size.y;
    int lz = pos.z - chunk_pos.z * voxel_grid->chunk_size.z;

    glm::ivec3 local_voxel_pos = glm::ivec3(lx, ly, lz);

    auto chunk_map_it = edited_voxels.find(chunk_key);
    if (chunk_map_it != edited_voxels.end()) {
        auto voxel_map = chunk_map_it->second;

        uint64_t local_voxel_key = VoxelGrid::pack_key(lx, ly, lz);
        auto voxel_it = voxel_map.find(local_voxel_key);
        if (voxel_it != voxel_map.end())
            return voxel_it->second;
    }

    auto chunk_it = voxel_grid->chunks.find(chunk_key);

    if (chunk_it == voxel_grid->chunks.end())
        return Voxel();

    size_t voxel_id = Chunk::idx(local_voxel_pos, voxel_grid->chunk_size);

    auto cur = std::atomic_load(&chunk_it->second->voxels);
    if (!cur || voxel_id >= cur->size())
        return Voxel();

    return (*cur)[voxel_id];
}