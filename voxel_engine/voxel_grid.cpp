#include "voxel_grid.h"

VoxelGrid::VoxelGrid(glm::ivec3 chunk_size, glm::ivec3 chunk_render_size) {
    this->chunk_render_size = chunk_render_size;
    this->chunk_size = chunk_size;

    mesh_thread_running = true;
    gen_thread_running = true;
    // mesh_updating_thread = std::thread(&VoxelGrid::mesh_worker_loop, this);

    unsigned n = std::thread::hardware_concurrency();
    if (n == 0) n = 4;
    n = std::max(1u, n - 1);

    mesh_workers.reserve(n);
    for (int i = 0; i < n; ++i)
        mesh_workers.emplace_back(&VoxelGrid::mesh_worker_loop, this);
    
    gen_workers.reserve(n);
    for (int i = 0; i < n; ++i)
        gen_workers.emplace_back(&VoxelGrid::gen_worker_loop, this);
}

VoxelGrid::~VoxelGrid() {
    mesh_thread_running = false;
    jobs_cv.notify_all();

    gen_thread_running = false;
    gen_jobs_cv.notify_all();

    for (auto& t : mesh_workers)
        if (t.joinable())
            t.join();
    
    for (auto& t : gen_workers)
        if (t.joinable())
            t.join();

    // if (mesh_updating_thread.joinable())
    //     mesh_updating_thread.join();
}

bool VoxelGrid::enqueue_mesh_job(uint64_t key, glm::ivec3 cpos, Chunk* chunk) {
    if (!chunk) return false;

    std::lock_guard<std::mutex> lk(jobs_mx);

    if (in_flight.find(key) != in_flight.end())
        return false;
    
    uint32_t rev = chunk->revision.load(std::memory_order_relaxed);

    auto snap_at = [&](glm::ivec3 ncpos) -> std::shared_ptr<const std::vector<Voxel>> {
        uint64_t k = pack_key(ncpos.x, ncpos.y, ncpos.z);
        auto it = chunks.find(k);
        if (it == chunks.end()) return {};
        return std::atomic_load(&it->second->voxels);
    };

    MeshJob job = MeshJob();
    job.key = key;
    job.cpos = cpos;
    job.chunk_size = chunk->size;
    job.revision = rev;
    // job.ticket = chunk->mesh_ticket.fetch_add(1, std::memory_order_relaxed) + 1;

    job.self = std::atomic_load(&chunk->voxels);
    
    job.nb[(int)Face::Left]   = snap_at(cpos + glm::ivec3{-1,0,0});
    job.nb[(int)Face::Right]  = snap_at(cpos + glm::ivec3{ 1,0,0});
    job.nb[(int)Face::Back]   = snap_at(cpos + glm::ivec3{0,0,-1});
    job.nb[(int)Face::Front]  = snap_at(cpos + glm::ivec3{0,0, 1});
    job.nb[(int)Face::Top]    = snap_at(cpos + glm::ivec3{0, 1,0});
    job.nb[(int)Face::Bottom] = snap_at(cpos + glm::ivec3{0,-1,0});

    
    in_flight.insert(key);
    jobs.push_back(job);
    jobs_cv.notify_one();

    return true;
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

        MeshData mesh_data = Chunk::build(*job.self, job.nb, job.chunk_size);
        
        {
            std::unique_lock<std::mutex> lk(results_mx);
            results.push_back(MeshResult{job.key, std::move(mesh_data), job.revision, job.ticket});
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
        auto it = chunks.find(r.key);

        if (it == chunks.end())
            continue;

        Chunk* chunk = it->second;
        uint32_t cur_rev = chunk->revision.load(std::memory_order_relaxed);

        // uint32_t cur_ticket = chunk->mesh_ticket.load(std::memory_order_relaxed);
        if (r.revision != cur_rev)
            continue;

        // if (r.ticket != cur_ticket)
        //     continue;
        
        // if (r.key == test_chunk_key)
        //     std::cout << "Updating the GPU mesh for the scheduled chunk" << std::endl;
        
        it->second->upload_mesh_gpu(r.mesh_data);
    }
}

static inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

static inline float u01(uint32_t h) {
    return (h & 0x00FFFFFF) / 16777216.0f; // [0,1)
}

static inline int floordiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

static inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

// Returns {F1, F2} in "cell units" (not normalized), using jittered points in grid cells.
static inline void worley_F1F2(float x, float z, float cell_size, float& outF1, float& outF2) {
    int cx = floordiv((int)std::floor(x), (int)cell_size);
    int cz = floordiv((int)std::floor(z), (int)cell_size);

    float F1 = 1e9f, F2 = 1e9f;

    // Search neighboring cells (needed because nearest point may live next door)
    for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx) {
        int ncx = cx + dx;
        int ncz = cz + dz;

        uint32_t h = hash_u32((uint32_t)ncx * 73856093u ^ (uint32_t)ncz * 19349663u ^ 0xA53C9E7Bu);

        // Jitter point within the cell
        float jx = u01(hash_u32(h ^ 0x1234u));
        float jz = u01(hash_u32(h ^ 0xBEEFu));

        float px = (float)ncx * cell_size + jx * cell_size;
        float pz = (float)ncz * cell_size + jz * cell_size;

        float dxw = x - px;
        float dzw = z - pz;
        float d = std::sqrt(dxw * dxw + dzw * dzw);

        if (d < F1) { F2 = F1; F1 = d; }
        else if (d < F2) { F2 = d; }
    }

    outF1 = F1;
    outF2 = F2;
}

std::shared_ptr<std::vector<Voxel>> VoxelGrid::generate_chunk(glm::ivec3 chunk_pos, glm::ivec3 chunk_size) {
    auto voxels = std::make_shared<std::vector<Voxel>>(
        (size_t)chunk_size.x * (size_t)chunk_size.y * (size_t)chunk_size.z
    );

    // ---------- EXTREME Voronoi/Worley terrain params ----------
    // Bigger cell_size => bigger craters/cells
    const float cell_size_1 = 48.0f;   // main scale
    const float cell_size_2 = 18.0f;   // detail scale

    // How deep basins and how high ridges (in voxels)
    const float basin_amp_1 = 18.0f;
    const float ridge_amp_1 = 14.0f;
    const float basin_amp_2 = 6.0f;
    const float ridge_amp_2 = 4.0f;

    // Basin radius relative to cell size (0..0.5 looks good)
    const float basin_radius_frac = 0.48f;

    // Ridge sharpness: smaller -> thinner, sharper ridges
    const float ridge_width_1 = 0.18f; // in *cell* units, relative-ish
    const float ridge_width_2 = 0.14f;

    // Make craters more “punchy”
    const float basin_pow = 2.4f; // >1 deepens center, steepens walls

    for (int vx = 0; vx < chunk_size.x; ++vx)
    for (int vy = 0; vy < chunk_size.y; ++vy)
    for (int vz = 0; vz < chunk_size.z; ++vz) {
        glm::ivec3 local_pos(vx, vy, vz);
        int id = (int)Chunk::idx(local_pos, chunk_size);

        int gx = vx + chunk_pos.x * chunk_size.x;
        int gy = vy + chunk_pos.y * chunk_size.y;
        int gz = vz + chunk_pos.z * chunk_size.z;

        // ----- base height (keep yours, but reduce it a bit so craters dominate) -----
        float wave_1 = (std::sin(gx / (float)chunk_size.x) + 1.0f) * 0.5f;
        float wave_2 = (std::cos(gz / (float)chunk_size.x) + 1.0f) * 0.5f;
        float base_h = ((wave_1 + wave_2) * 0.5f) * (float)chunk_size.y * 0.55f;

        // ----- Worley/Voronoi shaping in XZ -----
        auto eval_layer = [&](float cell_size, float basin_amp, float ridge_amp, float ridge_width) {
            float F1, F2;
            worley_F1F2((float)gx, (float)gz, cell_size, F1, F2);

            // Basin: near the nearest point -> deep crater
            float R = basin_radius_frac * cell_size;
            float basin_t = 1.0f - clamp01(F1 / R);
            float basin = std::pow(basin_t, basin_pow); // punchier center

            // Ridge: near cell borders, F2-F1 becomes small.
            // Convert to "edge strength" where 1 = on border, 0 = far from border.
            float edge = 1.0f - clamp01((F2 - F1) / (ridge_width * cell_size));
            // Sharpen ridges a lot
            float ridge = edge * edge * edge;

            // Net displacement: subtract basin (down), add ridge (up)
            return ridge_amp * ridge - basin_amp * basin;
        };

        float disp = 0.0f;
        disp += eval_layer(cell_size_1, basin_amp_1, ridge_amp_1, ridge_width_1);
        disp += eval_layer(cell_size_2, basin_amp_2, ridge_amp_2, ridge_width_2);

        float h = base_h + disp;

        int y_threshold = (int)std::floor(h);
        int diff = gy - y_threshold;

        if (diff <= 0) {
            (*voxels)[id].visible = true;

            // Color hint: ridges lighter, basins darker (cheap look improvement)
            float shade = 0.25f + 0.02f * (h - (float)gy);
            shade = std::clamp(shade, 0.18f, 0.65f);
            (*voxels)[id].color = {shade, shade, shade};
        }
    }

    return voxels;
}


void VoxelGrid::gen_worker_loop() {
    while(gen_thread_running) {
        GenJob gen_job;
        {
            std::unique_lock<std::mutex> lk(gen_jobs_mx);
            gen_jobs_cv.wait(lk, [&]{ return !gen_thread_running || !gen_jobs.empty(); });

            if (!gen_thread_running) 
                break;
            
            gen_job = gen_jobs.front();
            gen_jobs.pop_front();
        }

        GenResult gen_result;

        gen_result.cpos = gen_job.cpos;
        gen_result.key = gen_job.key;
        gen_result.voxels = generate_chunk(gen_job.cpos, gen_job.chunk_size);

        {
            std::unique_lock<std::mutex> lk(gen_results_mx);
            gen_results.push_back(std::move(gen_result));
        }
    }
}

void VoxelGrid::enqueue_gen_job(uint64_t key, glm::ivec3 cpos, glm::ivec3 chunk_size) {
    std::unique_lock<std::mutex> lk(gen_jobs_mx);

    GenJob gen_job;
    gen_job.key = key;
    gen_job.cpos = cpos;
    gen_job.chunk_size = chunk_size;

    gen_jobs.push_back(gen_job);
    gen_jobs_cv.notify_one();
}

void VoxelGrid::drain_gen_results() {
    std::deque<GenResult> local;
    {
        std::unique_lock<std::mutex> lk(gen_results_mx);
        local.swap(gen_results);
    }

    for (auto& r: local) {
        auto it = chunks.find(r.key);
        if (it == chunks.end())
            continue;
        
        Chunk* chunk = it->second;

        chunk->update_voxels(r.voxels);
        chunks_to_update.insert(r.key);
        chunks_to_update.insert(pack_key(r.cpos.x-1, r.cpos.y, r.cpos.z)); // left
        chunks_to_update.insert(pack_key(r.cpos.x, r.cpos.y, r.cpos.z-1)); // back
        chunks_to_update.insert(pack_key(r.cpos.x+1, r.cpos.y, r.cpos.z)); // right
        chunks_to_update.insert(pack_key(r.cpos.x, r.cpos.y, r.cpos.z+1)); // front
        chunks_to_update.insert(pack_key(r.cpos.x, r.cpos.y+1, r.cpos.z)); // top
        chunks_to_update.insert(pack_key(r.cpos.x, r.cpos.y-1, r.cpos.z)); // bottom
    }
}

bool VoxelGrid::is_voxel_free(glm::ivec3 pos) {
    int cx = pos.x / chunk_size.x + (pos.x % chunk_size.x < 0 ? -1 : 0);
    int cy = pos.y / chunk_size.y + (pos.y % chunk_size.y < 0 ? -1 : 0);
    int cz = pos.z / chunk_size.z + (pos.z % chunk_size.z < 0 ? -1 : 0);
 
    int lx = pos.x - cx * chunk_size.x;
    int ly = pos.y - cy * chunk_size.y;
    int lz = pos.z - cz * chunk_size.z;

    uint64_t key = pack_key(cx, cy, cz);

    auto it = chunks.find(key);
    if (it == chunks.end())
        return true;
    
    
    Chunk* chunk = it->second;

    auto v = std::atomic_load(&chunk->voxels);
    glm::ivec3 local_pos = glm::ivec3(lx, ly, lz);

    return Chunk::is_free(*v, local_pos, chunk_size);
}

void VoxelGrid::update(Window* window, Camera* camera) {
    glm::vec3 cam_pos = camera->position;
    glm::ivec3 center_voxel_pos = (glm::ivec3)cam_pos;
    glm::ivec3 center_chunk_pos = glm::ivec3(0, 0, 0);
    center_chunk_pos.x = cam_pos.x / chunk_size.x + ((int)cam_pos.x % chunk_size.x < 0 ? -1 : 0);
    center_chunk_pos.y = cam_pos.y / chunk_size.y + ((int)cam_pos.y % chunk_size.y < 0 ? -1 : 0);
    center_chunk_pos.z = cam_pos.z / chunk_size.z + ((int)cam_pos.z % chunk_size.z < 0 ? -1 : 0);

    // glm::ivec3 center_chunk_pos = glm::ivec3(cam_pos.x / chunk_size.x, cam_pos.y / chunk_size.y, cam_pos.z / chunk_size.z);

    glm::ivec3 front_left_bottom_chunk_pos = center_chunk_pos - chunk_render_size / 2;
    bool to_update_mesh = false;

    for (int x = 0; x < chunk_render_size.x; x++)
        for (int y = 0; y < chunk_render_size.y; y++)
            for (int z = 0; z < chunk_render_size.z; z++) {
                glm::ivec3 cpos = front_left_bottom_chunk_pos + glm::ivec3(x, y, z);
                uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);

                auto it = chunks.find(key);

                if (it != chunks.end())
                    continue;
                else {
                    Chunk* new_chunk = new Chunk(chunk_size, {1, 1, 1});
                    new_chunk->position = glm::vec3(cpos.x * chunk_size.x, cpos.y * chunk_size.y, cpos.z * chunk_size.z);
                    chunks[key] = new_chunk;
                    enqueue_gen_job(key, cpos, chunk_size);
                }
                    

                
                // Chunk* chunk_to_draw = nullptr;

                // uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);
                // auto it = chunks.find(key);
                // if (it == chunks.end()) {
                //     to_update_mesh = true;
                //     Chunk* new_chunk = new Chunk(chunk_size, {1, 1, 1});
                    
                //     new_chunk->position = glm::vec3(cpos.x * chunk_size.x, cpos.y * chunk_size.y, cpos.z * chunk_size.z);
                //     // float r = (rand() % 255) / 255.0f;
                //     // float g = (rand() % 255) / 255.0f;
                //     // float b = (rand() % 255) / 255.0f;
                //     // glm::vec3 color = {r, g, b};

                //     edit_chunk(cpos, new_chunk, [&](std::vector<Voxel>& voxels){
                //         for (int vx = 0; vx < new_chunk->size.x; vx++)
                //             for (int vy = 0; vy < new_chunk->size.y; vy++)
                //                 for (int vz = 0; vz < new_chunk->size.z; vz++) {
                //                     glm::ivec3 local_pos = glm::ivec3(vx, vy, vz);
                //                     int id = Chunk::idx(local_pos, chunk_size);

                //                     int gx = (float)vx + (float)cpos.x * chunk_size.x;
                //                     int gy = (float)vy + (float)cpos.y * chunk_size.y;
                //                     int gz = (float)vz + (float)cpos.z * chunk_size.z;
                                    
                //                     float wave_1 = (sin(gx / (float)chunk_size.x)+1.0)/2.0;
                //                     float wave_2 = (cos(gz / (float)chunk_size.x)+1.0)/2.0;

                //                     float final_wave = (wave_1 + wave_2)/2.0;

                //                     int y_threshold = (int)(final_wave * chunk_size.y);

                //                     glm::vec3 color_1 = {0.8, 0.1, 0.1};
                //                     glm::vec3 color_2 = {0.1, 0.1, 0.8};

                //                     glm::vec3 color = {0.0, 0.0, 0.0};
                //                     if (cpos.z % 2 == 0)
                //                         if (cpos.x % 2 == 0)
                //                             color = color_2;
                //                         else
                //                             color = color_1;
                //                     else
                //                         if (cpos.x % 2 != 0)
                //                             color = color_2;
                //                         else
                //                             color = color_1;


                                    
                //                     if (gy <= y_threshold) {
                //                         voxels[id].visible = true;
                //                         // new_chunk->voxels[new_chunk->idx(vx, vy, vz)].color = {final_wave, 0.0, 0.0};
                                        
                //                         voxels[id].color = color;
                //                     }
                //                 }
                //     });
                // }
            }
    
    drain_gen_results();
    
    for (auto it = chunks_to_update.begin(); it != chunks_to_update.end(); ) {
        uint64_t key = *it;

        auto it_chunk = chunks.find(key);
        if (it_chunk == chunks.end()) {
            it = chunks_to_update.erase(it);
            continue;
        }

        glm::ivec3 cpos = unpack_key(key);

        if (enqueue_mesh_job(key, cpos, it_chunk->second))
            it = chunks_to_update.erase(it);
        else
            ++it;
    }
    
    
    drain_mesh_results();
}

void VoxelGrid::draw(RenderState state) {
    state.transform *= get_model_matrix();

    glm::vec3 cam_pos = state.camera->position;
    glm::ivec3 center_chunk_pos = glm::ivec3(0, 0, 0);
    center_chunk_pos.x = cam_pos.x / chunk_size.x + ((int)cam_pos.x % chunk_size.x < 0 ? -1 : 0);
    center_chunk_pos.y = cam_pos.y / chunk_size.y + ((int)cam_pos.y % chunk_size.y < 0 ? -1 : 0);
    center_chunk_pos.z = cam_pos.z / chunk_size.z + ((int)cam_pos.z % chunk_size.z < 0 ? -1 : 0);

    glm::ivec3 front_left_bottom_chunk_pos = center_chunk_pos - chunk_render_size / 2;

    for (int x = 0; x < chunk_render_size.x; x++)
        for (int y = 0; y < chunk_render_size.y; y++)
            for (int z = 0; z < chunk_render_size.z; z++) {
                glm::ivec3 cpos = front_left_bottom_chunk_pos + glm::ivec3(x, y, z);

                glm::vec3 bmin = glm::vec3(cpos) * glm::vec3(chunk_size);
                glm::vec3 bmax = bmin + glm::vec3(chunk_size);

                if (!state.camera->visible_AABB(bmin, bmax)) // frustum culling
                    continue;

                uint64_t key = pack_key(cpos.x, cpos.y, cpos.z);
                auto it = chunks.find(key);
                if (it != chunks.end()) {
                    Chunk* chunk_to_draw = it->second;            
                    chunk_to_draw->draw(state);
                }
            }
}