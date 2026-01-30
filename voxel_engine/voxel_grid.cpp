#include "voxel_grid.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

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

// ---------- deterministic 2D value noise + FBM ----------
static inline float lerp_f(float a, float b, float t) { return a + (b - a) * t; }
static inline float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }

static inline uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static inline float rand01_2d(int x, int z, uint32_t seed) {
    uint32_t h = hash_u32((uint32_t)x * 73856093U ^ (uint32_t)z * 19349663U ^ seed * 83492791U);
    return (h & 0x00FFFFFFu) / 16777216.0f; // [0,1)
}

static inline float value_noise2d(float x, float z, uint32_t seed) {
    int x0 = (int)std::floor(x), z0 = (int)std::floor(z);
    int x1 = x0 + 1,           z1 = z0 + 1;

    float tx = x - (float)x0;
    float tz = z - (float)z0;

    float u = fade(tx);
    float v = fade(tz);

    float a = rand01_2d(x0, z0, seed);
    float b = rand01_2d(x1, z0, seed);
    float c = rand01_2d(x0, z1, seed);
    float d = rand01_2d(x1, z1, seed);

    float ab = lerp_f(a, b, u);
    float cd = lerp_f(c, d, u);
    return (lerp_f(ab, cd, v) * 2.0f - 1.0f); // [-1,1]
}

static inline float fbm2d(float x, float z, uint32_t seed, int octaves,
                          float lacunarity = 2.0f, float gain = 0.5f)
{
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum  += amp * value_noise2d(x * freq, z * freq, seed + (uint32_t)i * 1013u);
        norm += amp;
        freq *= lacunarity;
        amp  *= gain;
    }
    return (norm > 0.0f) ? (sum / norm) : 0.0f; // ~[-1,1]
}

static inline float saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }
static inline float smoothstep(float a, float b, float x) {
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
static inline glm::vec3 lerp_v3(const glm::vec3& a, const glm::vec3& b, float t) {
    return a * (1.0f - t) + b * t;
}
// -------------------------------------------------------

std::shared_ptr<std::vector<Voxel>> VoxelGrid::generate_chunk(glm::ivec3 chunk_pos, glm::ivec3 chunk_size) {
    auto voxels = std::make_shared<std::vector<Voxel>>(
        (size_t)chunk_size.x * (size_t)chunk_size.y * (size_t)chunk_size.z
    );

    const int base_x = chunk_pos.x * chunk_size.x;
    const int base_y = chunk_pos.y * chunk_size.y;
    const int base_z = chunk_pos.z * chunk_size.z;

    // --- Terrain params (these actually matter a lot) ---
    const uint32_t SEED = 1337u;

    const float freq_low  = 0.006f;   // big hills
    const float freq_high = 0.03f;    // detail
    const int   oct_low   = 5;
    const int   oct_high  = 3;

    const float height_amp = (float)chunk_size.y * 2.2f; // overall relief in voxel-y units
    const float sea_level  = 0.0f;

    // Heightmap in GLOBAL coordinates (gx,gz) -> height in GLOBAL voxel-y
    auto height_at = [&](int gx, int gz) -> float {
        float n1 = fbm2d((float)gx * freq_low,  (float)gz * freq_low,  SEED,     oct_low);  // ~[-1,1]
        float n2 = fbm2d((float)gx * freq_high, (float)gz * freq_high, SEED+999, oct_high); // ~[-1,1]

        // Convert to [0,1], then shape a bit
        float h1 = n1 * 0.5f + 0.5f;
        float h2 = n2 * 0.5f + 0.5f;

        // Add ridged effect for sharper features
        float ridged = 1.0f - std::abs(n2);   // [0,1]
        ridged = ridged * ridged;

        float h = sea_level
                + h1 * height_amp                 // big shapes
                + (h2 - 0.5f) * (height_amp * 0.35f) // medium variation
                + ridged * (height_amp * 0.25f);  // sharper ridges

        return h;
    };

    // cosUp = dot(normal, up) for y=h(x,z)
    auto cos_up_at = [&](int gx, int gz) -> float {
        // central differences (in voxel units; if your voxel size isn't 1, scale here)
        float hx = 0.5f * (height_at(gx + 1, gz) - height_at(gx - 1, gz));
        float hz = 0.5f * (height_at(gx, gz + 1) - height_at(gx, gz - 1));

        // cosUp = 1/sqrt(1+hx^2+hz^2)
        return 1.0f / std::sqrt(1.0f + hx * hx + hz * hz); // (0,1]
    };

    // Colors based ONLY on cosUp (flat -> grass, steep -> rock)
    const glm::vec3 grass = {0.20f, 0.60f, 0.25f};
    const glm::vec3 rock  = {0.65f, 0.65f, 0.65f};
    const glm::vec3 dirt  = {0.18f, 0.14f, 0.10f};

    // This thresholding is what prevents "everything looks the same"
    // cosUp ~ 1 is flat. Below ~0.85 starts to get rocky.
    const float cos_flat_start = 0.92f;
    const float cos_steep_end  = 0.75f;

    const glm::vec3 road_color = {0.2f, 0.2f, 0.2f};
    // const glm::vec3 road_color = {1.0f, 1.0f, 1.0f};
    const glm::vec3 curvature_color = {1.0f, 0.0f, 0.0f};
    // const glm::vec3 curvature_color = {0.0f, 0.0f, 1.0f};

    for (int vx = 0; vx < chunk_size.x; ++vx)
    for (int vz = 0; vz < chunk_size.z; ++vz) {
        int gx = base_x + vx;
        int gz = base_z + vz;

        float h = height_at(gx, gz);
        int y_surface = (int)std::floor(h);
        // int y_surface = 0;

        float cosUp = cos_up_at(gx, gz); // <-- the value you asked for

        // map cosUp -> steepness in [0,1] with visible contrast
        // flat: cosUp >= cos_flat_start -> steep=0
        // steep: cosUp <= cos_steep_end  -> steep=1
        float steep = 1.0f - smoothstep(cos_steep_end, cos_flat_start, cosUp);

        glm::vec3 surface_col = lerp_v3(grass, rock, steep);

        for (int vy = 0; vy < chunk_size.y; ++vy) {
            int gy = base_y + vy;
            int id = (int)Chunk::idx({vx, vy, vz}, chunk_size);

            if (gy <= y_surface) {
                (*voxels)[id].visible = true;
                (*voxels)[id].color = lerp_v3(curvature_color, road_color, cosUp);
                if (cosUp <= 0.8)
                    (*voxels)[id].color = curvature_color;
                
                (*voxels)[id].curvature = 1.0f - cosUp;
                

                // if (gy == y_surface) {
                //     (*voxels)[id].color = surface_col;

                //     // DEBUG (uncomment to *see* cosUp directly as grayscale):
                //     // (*voxels)[id].color = glm::vec3(cosUp);
                // } else {
                //     // underground: darker dirt-ish
                //     glm::vec3 underground = lerp_v3(dirt, surface_col, 0.25f);
                //     (*voxels)[id].color = underground * 0.7f;
                // }
            }
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

bool VoxelGrid::get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop) {
    for (int y = 0; y < max_drop; y++) {
        glm::ivec3 cur_pos = pos - glm::ivec3(0, y, 0);
        Voxel voxel = get_voxel(cur_pos);
        if (voxel.visible) {
            result = cur_pos;
            return true;
        }
    }
    return false;
}

Voxel VoxelGrid::get_voxel(glm::ivec3 pos) {
    glm::ivec3 chunk_pos = VoxelGrid::get_chunk_pos(pos, chunk_size);
    uint64_t chunk_key = VoxelGrid::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);
    
    auto it = chunks.find(chunk_key);
    if (it == chunks.end()) {
        Voxel air;
        air.visible = false;
        return air;
    }

    Chunk* chunk = it->second;

    int lx = pos.x - chunk_pos.x * chunk_size.x;
    int ly = pos.y - chunk_pos.y * chunk_size.y;
    int lz = pos.z - chunk_pos.z * chunk_size.z;

    glm::ivec3 local_voxel_pos = glm::ivec3(lx, ly, lz);

    size_t voxel_id = chunk->idx(local_voxel_pos, chunk->size);

    return (*chunk->voxels)[voxel_id];


    
    // // uint64_t local_voxel_key = VoxelGrid::pack_key(lx, ly, lz);
    // uint32_t local_voxel_key = pack_local_id(local_voxel_pos, voxel_grid->chunk_size);

    // auto [it, inserted] = edited_voxels.try_emplace(chunk_key);
    // // if you have a guess, reserve once:
    // // if (inserted) it->second.reserve(256);
    // it->second[local_voxel_key] = voxel;
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