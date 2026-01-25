#include "nonholonomic_a_star.h"


std::vector<NonholonomicPos> NonholonomicAStar::simulate_motion(NonholonomicPos start, int steer)
{
    steer = std::clamp(steer, -1, 1);

    // Your convention: steer = -1 => max LEFT, steer = +1 => max RIGHT.
    // Typically +delta is left, so:
    float delta = 0.0f;
    if (steer != 0) delta = -steer * max_steer; // -1 -> +max_steer, +1 -> -max_steer

    std::vector<NonholonomicPos> out;
    out.reserve(integration_steps);

    glm::vec3 p = start.pos;
    float yaw   = start.theta.y;

    const float ds = motion_simulation_dist / float(integration_steps);
    const float eps = 1e-6f;

    for (int i = 0; i < integration_steps; ++i) {
        if (std::abs(delta) < eps) {
            // straight
            p.x += ds * std::cos(yaw);
            p.z += ds * std::sin(yaw);
        } else {
            // exact circular arc integration for constant steering
            float R = wheel_base / std::tan(delta); // turning radius (signed)
            float yaw0 = yaw;
            float dYaw = ds / R;                    // because ds = R * dYaw
            yaw += dYaw;

            p.x += R * (std::sin(yaw) - std::sin(yaw0));
            p.z += R * (-std::cos(yaw) + std::cos(yaw0));
        }

        NonholonomicPos s = start;
        s.pos = p;
        s.theta.y = yaw;
        out.push_back(s);
    }

    return out;
}

std::vector<glm::ivec3> NonholonomicAStar::find_nonholomic_path(NonholonomicPos start_pos, NonholonomicPos end_pos) {
    std::priority_queue<AStarCell, std::vector<AStarCell>, ByPriority> pq;
    std::unordered_map<uint64_t, AStarCell> closed_heap;
    std::unordered_map<uint64_t, float> g_score;
    
    AStarCell start;
    start.pos = start_pos.pos;
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

        if (cur_cell.pos == (glm::ivec3)end_pos.pos) {
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

                if (grid->get_cell(new_pos).solid) {
                    if (grid->get_cell(new_pos + glm::ivec3(0, 1, 0)).solid)
                        continue;
                    new_pos.y += 1;
                }
                else {
                    if (!grid->get_cell(new_pos + glm::ivec3(0, -1, 0)).solid)
                        if (!grid->get_cell(new_pos + glm::ivec3(0, -2, 0)).solid)
                            continue;
                        else {
                            new_pos.y -= 1;
                        }
                }

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
                new_cell.f = new_g + get_heuristic(new_pos, end_pos.pos);
       
                pq.push(new_cell);
            }
    }
    return {};
}