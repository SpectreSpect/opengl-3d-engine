#include "a_star.h"


AStar::AStar() {
    this->grid = new OccupancyGrid3D();
}

AStar::AStar(VoxelGrid* voxel_grid) {
    this->grid = new VoxelOccupancyGrid3D(voxel_grid);
}

float AStar::get_heuristic(glm::ivec3 a, glm::ivec3 b) {
    glm::vec3 d = glm::vec3(a - b);
    return glm::dot(d, d);
}

std::vector<glm::ivec3> AStar::reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos) {
    std::vector<glm::ivec3> path;
    path.push_back(pos);
    glm::ivec3 cur_pos = pos;

    while (true) {
        uint64_t cur_key = grid->pack_key(cur_pos.x, cur_pos.y, cur_pos.z);
        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        
        AStarCell prev_cell = it->second;

        if (prev_cell.no_parent)
            break;
        
        
        path.push_back(cur_pos);
        cur_pos = prev_cell.came_from;
    }

    std::reverse(path.begin(), path.end());

    return path;
}

bool AStar::adjust_to_ground(glm::ivec3& voxel_pos, int max_step_up, int max_drop) {
    auto solid = [&](const glm::ivec3& q) {
        return grid->get_cell(q).solid;
    };

    // 1) If we're inside solid, try stepping up
    if (solid(voxel_pos)) {
        bool freed = false;
        for (int k = 1; k <= max_step_up; ++k) {
            glm::ivec3 up = voxel_pos + glm::ivec3(0, k, 0);
            if (!solid(up)) {
                voxel_pos = up;
                freed = true;
                break;
            }
        }
        if (!freed) return false;
    }

    // 2) Now find a y such that: current is empty AND below is solid
    // (and don't drop more than max_drop)
    for (int drop = 0; drop <= max_drop; ++drop) {
        if (!solid(voxel_pos) && solid(voxel_pos + glm::ivec3(0, -1, 0)))
            return true;

        // If we somehow are in solid, we're already too low → reject
        // (or you could step up 1, but reject is safer)
        if (solid(voxel_pos))
            return false;

        voxel_pos.y -= 1;
    }

    return false;
}

// float my_smoothstep(float e0, float e1, float x) {
//     float t = glm::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
//     return t * t * (3.0f - 2.0f * t);
// }

// float directional_peak(glm::vec3 a_in, glm::vec3 b_in, glm::vec3 p_in,
//                        float sigma, float halfAngleRad, float sharpnessK,
//                        bool suppressAlongPlusD = true)
// {
//     glm::vec2 a = glm::vec2(a_in.x, a_in.z);
//     glm::vec2 b = glm::vec2(b_in.x, b_in.z);
//     glm::vec2 p = glm::vec2(p_in.x, p_in.z);

//     glm::vec2 d = glm::normalize(a - b);      // "bad" direction
//     glm::vec2 r = p - a;
//     float dist = glm::length(r);

//     float R = std::exp(- (dist*dist) / (sigma*sigma));

//     if (dist < 1e-8f) return R;               // at a: purely radial peak

//     glm::vec2 u = r / dist;
//     float c = glm::dot(u, suppressAlongPlusD ? d : -d);

//     float ca = std::cos(halfAngleRad);
//     float N = 1.0f - my_smoothstep(ca, 1.0f, c); // 1 outside cone, 0 inside
//     return R * std::pow(N, sharpnessK);
// }

std::vector<glm::ivec3> AStar::find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) {
    std::priority_queue<AStarCell, std::vector<AStarCell>, ByPriority> pq;
    std::unordered_map<uint64_t, AStarCell> closed_heap;
    std::unordered_map<uint64_t, float> g_score;
    
    AStarCell start;
    start.pos = start_pos;
    start.no_parent = true;
    start.g = 0;
    start.f = 0;

    int limit = 10000;
    int counter = 0;

    pq.push(start);

    while (!pq.empty()) {
        AStarCell cur_cell = pq.top();
        pq.pop();

        if (counter >= limit)
            return {};

        counter++;
        
        uint64_t cur_key = grid->pack_key(cur_cell.pos.x, cur_cell.pos.y, cur_cell.pos.z);
        closed_heap[cur_key] = cur_cell;

        if (cur_cell.pos == end_pos) {
            return reconstruct_path(closed_heap, cur_cell.pos);
        }
            
        for (int dx = -1; dx <= 1; dx++)
            for (int dz = -1; dz <= 1; dz++) {

                if (dx == 0 && dz == 0)
                    continue;

                int nx = dx + cur_cell.pos.x;
                int ny = cur_cell.pos.y;
                int nz = dz + cur_cell.pos.z;

                glm::ivec3 new_pos = glm::ivec3(nx, ny, nz);

                bool need_continue = false;
                if (!adjust_to_ground(new_pos)) {
                    need_continue = true;
                    break;
                }
                if (need_continue)
                    continue;


                // if (grid->get_cell(new_pos).solid) {
                //     if (grid->get_cell(new_pos + glm::ivec3(0, 1, 0)).solid)
                //         continue;
                //     new_pos.y += 1;
                // }
                // else {
                //     if (!grid->get_cell(new_pos + glm::ivec3(0, -1, 0)).solid)
                //         if (!grid->get_cell(new_pos + glm::ivec3(0, -2, 0)).solid)
                //             continue;
                //         else {
                //             new_pos.y -= 1;
                //         }
                // }

                uint64_t new_key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
                auto heap_it = closed_heap.find(new_key);
                if (heap_it != closed_heap.end())
                    continue;
                

                float new_g = cur_cell.g + glm::distance((glm::vec3)cur_cell.pos, (glm::vec3)new_pos);

                uint64_t key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
                auto it = g_score.find(key);
                
                if (it != g_score.end()) {
                    float old_g = it->second;
                    if (old_g <= new_g)
                        continue;
                }

                g_score[key] = new_g;

                AStarCell new_cell;
                new_cell.pos = new_pos;
                new_cell.came_from = cur_cell.pos;
                new_cell.no_parent = false;
                new_cell.g = new_g;
                // new_cell.f = new_g + get_heuristic(new_pos, end_pos) + directional_peak(end_pos, end_pos + glm::ivec3(1, 0, 0), new_pos, 5, 2, 2.8) * 10;
                new_cell.f = new_g + get_heuristic(new_pos, end_pos) * 10;
                


                
       
                pq.push(new_cell);
            }
    }
    return {};
}