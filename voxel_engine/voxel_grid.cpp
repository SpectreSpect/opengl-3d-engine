#include "voxel_grid.h"

VoxelGrid::VoxelGrid(glm::ivec3 chunk_size, int chunk_render_distance) {
    this->chunk_render_distance = chunk_render_distance;
    this->chunk_size = chunk_size;

    mesh_thread_running = true;
    mesh_updating_thread = std::thread(&VoxelGrid::mesh_worker_loop, this);
}

VoxelGrid::~VoxelGrid() {
    mesh_thread_running = false;
    jobs_cv.notify_all();
    if (mesh_updating_thread.joinable())
        mesh_updating_thread.join();
}

void VoxelGrid::enqueue_mesh_job(uint64_t key, glm::ivec3 cpos, Chunk* chunk) {
    if (!chunk) return;

    std::lock_guard<std::mutex> lk(jobs_mx);

    if (in_flight.find(key) != in_flight.end())
        return;
    
    in_flight.insert(key);
    jobs.push_back(MeshJob{key, cpos, chunk});
    jobs_cv.notify_one();
}

void VoxelGrid::mesh_worker_loop() {
    while (mesh_thread_running) {
        MeshJob job;

        {
            std::unique_lock<std::mutex> lk(jobs_mx);
            jobs_cv.wait(lk, [&]{ return !mesh_thread_running || !jobs.empty(); });

            if (!mesh_thread_running) 
                break;
            
            job = jobs.front();
            jobs.pop_front();
        }

        MeshData mesh = job.chunk->build_mesh_cpu_detahed(this, job.cpos);

        {
            std::unique_lock<std::mutex> lk(jobs_mx);
            results.push_back(MeshResult{job.key, job.chunk, std::move(mesh)});
        }

        {
            std::lock_guard<std::mutex> lk(jobs_mx);
            in_flight.erase(job.key);
        }
    }
}

void VoxelGrid::drain_mesh_results() {
    std::deque<MeshResult> local;
    {
        std::lock_guard<std::mutex> lk(results_mx);
        local.swap(results);
    }

    for (auto& r: local) {
        if (r.chunk)
            r.chunk->upload_mesh_gpu(std::move(r.mesh_data));
    }
}


void VoxelGrid::update_chunk_meshes() {
    
    for (auto it_upd = chunks_to_update.begin(); it_upd != chunks_to_update.end(); ) {
        uint64_t key = *it_upd;

        auto it_chunk = chunks.find(key);
        glm::ivec3 cpos = unpack_key(key);

        if (it_chunk == chunks.end()) {
            it_upd = chunks_to_update.erase(it_upd);           
            continue;
        }

        it_chunk->second->update_mesh(this, cpos);

        it_upd = chunks_to_update.erase(it_upd);
    }
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

void VoxelGrid::update(Camera* camera) {
    glm::vec3 cam_pos = camera->position;
    glm::ivec3 center_voxel_pos = (glm::ivec3)cam_pos;
    glm::ivec3 center_chunk_pos = (glm::ivec3){cam_pos.x / chunk_size.x, cam_pos.y / chunk_size.y, cam_pos.z / chunk_size.z};

    glm::ivec3 front_left_bottom_chunk_pos = center_chunk_pos - chunk_render_distance / 2;

    bool to_update_mesh = false;

    for (int x = 0; x < chunk_render_distance; x++)
        for (int y = 0; y < chunk_render_distance; y++)
            for (int z = 0; z < chunk_render_distance; z++) {
                glm::ivec3 cpos = front_left_bottom_chunk_pos + (glm::ivec3){x, y, z};
                Chunk* chunk_to_draw = nullptr;

                uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);
                auto it = chunks.find(key);
                if (it == chunks.end()) {
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

                    chunks[pack_key(cpos.x, cpos.y, cpos.z)] = new_chunk;

                    chunks_to_update.insert(pack_key(cpos.x, cpos.y, cpos.z)); // center
                    chunks_to_update.insert(pack_key(cpos.x-1, cpos.y, cpos.z)); // left
                    chunks_to_update.insert(pack_key(cpos.x, cpos.y, cpos.z-1)); // back
                    chunks_to_update.insert(pack_key(cpos.x+1, cpos.y, cpos.z)); // right 
                    chunks_to_update.insert(pack_key(cpos.x, cpos.y, cpos.z+1)); // front 
                    chunks_to_update.insert(pack_key(cpos.x, cpos.y+1, cpos.z)); // top 
                    chunks_to_update.insert(pack_key(cpos.x, cpos.y-1, cpos.z)); // bottom 
                }
            }
    
    for (auto it_upd = chunks_to_update.begin(); it_upd != chunks_to_update.end(); ) {
        uint64_t key = *it_upd;

        auto it_chunk = chunks.find(key);
        if (it_chunk != chunks.end()) {
            glm::ivec3 cpos = unpack_key(key);
            enqueue_mesh_job(key, cpos, it_chunk->second);
        }

        it_upd = chunks_to_update.erase(it_upd);
    }

    // then apply finished meshes (GPU upload) this frame:
    drain_mesh_results();

    
    // for (auto it_upd = chunks_to_update.begin(); it_upd != chunks_to_update.end();) {
    //     uint64_t key = *it_upd;

    //     auto it_chunk = chunks.find(key);
    //     glm::ivec3 cpos = unpack_key(key);

    //     if (it_chunk == chunks.end()) {
    //         it_upd = chunks_to_update.erase(it_upd);           
    //         continue;
    //     }

    //     it_chunk->second->update_mesh(this, cpos);

    //     it_upd = chunks_to_update.erase(it_upd);
    // }
}

void VoxelGrid::draw(RenderState state) {
    state.transform *= get_model_matrix();

    glm::vec3 cam_pos = state.camera->position;
    glm::ivec3 center_chunk_pos = (glm::ivec3){cam_pos.x / chunk_size.x, cam_pos.y / chunk_size.y, cam_pos.z / chunk_size.z};
    glm::ivec3 front_left_bottom_chunk_pos = center_chunk_pos - chunk_render_distance / 2;
    
    for (int x = 0; x < chunk_render_distance; x++)
        for (int y = 0; y < chunk_render_distance; y++)
            for (int z = 0; z < chunk_render_distance; z++) {
                glm::ivec3 cpos = front_left_bottom_chunk_pos + (glm::ivec3){x, y, z};
                uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);
                auto it = chunks.find(key);
                if (it != chunks.end()) {
                    Chunk* chunk_to_draw = it->second;
                    chunk_to_draw->draw(state);
                }
            }
}