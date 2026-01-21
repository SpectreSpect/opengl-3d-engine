#include "voxel_grid.h"

VoxelGrid::VoxelGrid(glm::ivec3 chunk_size, int chunk_render_distance) {
    this->chunk_render_distance = chunk_render_distance;
    this->chunk_size = chunk_size;
    
}

bool VoxelGrid::is_voxel_free(glm::ivec3 pos) {
    int cx = pos.x / chunk_size.x + (pos.x % chunk_size.x < 0 ? -1 : 0);
    int cy = pos.y / chunk_size.y + (pos.y % chunk_size.y < 0 ? -1 : 0);
    int cz = pos.z / chunk_size.z + (pos.z % chunk_size.z < 0 ? -1 : 0); // -1 -2 -3 -4 -5 -6 -7 -8 -9 -10

    // int lx = (pos.x % chunk_size.x + chunk_size.x) % chunk_size.x;
    // int ly = (pos.y % chunk_size.y + chunk_size.y) % chunk_size.y;
    // int lz = (pos.z % chunk_size.z + chunk_size.z) % chunk_size.z;
    
    int lx = pos.x - cx * chunk_size.x;
    int ly = pos.y - cy * chunk_size.y;
    int lz = pos.z - cz * chunk_size.z;

    uint64_t key = pack_key(cx, cy, cz);

    auto it = chunks.find(key);
    if (it == chunks.end())
        return true;
    
    
    Chunk* chunk = it->second;

    // int lx = (pos.x % chunk_size.x + chunk_size.x) % chunk_size.x;
    // int ly = (pos.y % chunk_size.y + chunk_size.y) % chunk_size.y;
    // int lz = (pos.z % chunk_size.z + chunk_size.z) % chunk_size.z;

    // glm::ivec3 pos_in_chunk = pos - chunk_pos * chunk_size;
    
    bool res = chunk->is_free({lx, ly, lz});
    // if (!res) {
    //     std::cout << 
    //     "g_voxel: (" << pos.x << ", " << pos.y << ", " << pos.z << ")  " <<
    //     "c_pos: (" << cx << ", " << cy << ", " << cz << ")  " << 
    //     "l_voxel: (" << lx << ", " << ly << ", " << lz << ")" << std::endl;
    // }

    return res;
}

void VoxelGrid::draw(RenderState state) {
    state.transform *= get_model_matrix();

    glm::vec3 cam_pos = state.camera->position;

    

    glm::ivec3 center_voxel_pos = (glm::ivec3)state.camera->position;
    glm::ivec3 center_chunk_pos = (glm::ivec3){cam_pos.x / chunk_size.x, cam_pos.y / chunk_size.y, cam_pos.z / chunk_size.z};

    

    glm::ivec3 front_left_bottom_chunk_pos = center_chunk_pos - chunk_render_distance / 2;


    // std::cout << front_left_bottom_chunk_pos.x << ", " << front_left_bottom_chunk_pos.y << ", " << front_left_bottom_chunk_pos.z << std::endl;

    bool to_update_mesh = false;

    for (int x = 0; x < chunk_render_distance; x++)
        for (int y = 0; y < chunk_render_distance; y++)
            for (int z = 0; z < chunk_render_distance; z++) {
                glm::ivec3 cpos = front_left_bottom_chunk_pos + (glm::ivec3){x, y, z};
                Chunk* chunk_to_draw = nullptr;

                uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);
                auto it = chunks.find(key);
                if (it != chunks.end()) {
                    chunk_to_draw = it->second;
                    chunk_to_draw->draw(state);
                } else {
                    to_update_mesh = true;
                    Chunk* new_chunk = new Chunk(chunk_size, {1, 1, 1});
                    
                    new_chunk->position = (glm::vec3){cpos.x * chunk_size.x, cpos.y * chunk_size.y, cpos.z * chunk_size.z};

                    for (int vx = 0; vx < new_chunk->size.x; vx++)
                        for (int vy = 0; vy < new_chunk->size.y; vy++)
                            for (int vz = 0; vz < new_chunk->size.z; vz++) {
                                int gx = (float)vx + (float)cpos.x * chunk_size.x;
                                int gy = (float)vy + (float)cpos.y * chunk_size.y;
                                int gz = (float)vz + (float)cpos.z * chunk_size.z;
                                
                                float wave_1 = (sin(gx / (float)chunk_size.x)+1.0)/2.0;
                                float wave_2 = (cos(gz / (float)chunk_size.x)+1.0)/2.0;

                                float final_wave = (wave_1 + wave_2)/2.0;

                                int y_threshold = (int)(final_wave * chunk_size.y);
                                
                                if (gy <= y_threshold) {
                                    new_chunk->voxels[new_chunk->idx(vx, vy, vz)].visible = true;
                                    new_chunk->voxels[new_chunk->idx(vx, vy, vz)].color = {final_wave, 0.0, 0.0};
                                }
                                
                                // if (gy < 2)
                                //     new_chunk->voxels[new_chunk->idx(vx, vy, vz)].visible = true;
                            }
                    // new_chunk->update_mesh();

                    chunks[pack_key(cpos.x, cpos.y, cpos.z)] = new_chunk;
                    chunk_to_draw = new_chunk;
                }
                // std::cout << chunk_to_draw << std::endl;
            }
    
    for (int x = 0; x < chunk_render_distance; x++)
        for (int y = 0; y < chunk_render_distance; y++)
            for (int z = 0; z < chunk_render_distance; z++) {
                glm::ivec3 cpos = front_left_bottom_chunk_pos + (glm::ivec3){x, y, z};
                

                uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);
                auto it = chunks.find(key);

                if (it == chunks.end())
                    continue;

                Chunk* chunk_to_draw = it->second;
                    
                if (to_update_mesh)
                    chunk_to_draw->update_mesh(this, cpos);
                chunk_to_draw->draw(state);
            }
    

    // test_chunk
    // std::cout << 'a' << std::endl;
    // test_chunk = new Chunk(chunk_size, {1, 1, 1});
    // test_chunk->draw(state);

    // mesh->draw(state);
}