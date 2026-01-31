#include "curvature_a_star.h"


PlainAstarData CurvatureAStar::find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) {
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

        if (counter >= limit) {
            std::cout << "LIMIT EXCEEDED" << std::endl;
            return {};
        }
            

        uint64_t cur_key = grid->pack_key(cur_cell.pos.x, cur_cell.pos.y, cur_cell.pos.z);
        auto cur_it = g_score.find(cur_key);

        if (cur_it != g_score.end())
            if (cur_cell.g > cur_it->second)
                continue;
        
        closed_heap[cur_key] = cur_cell;

        counter++;
        // std::cout << "(" << cur_cell.pos.x << ", " << cur_cell.pos.y << ", " << cur_cell.pos.z << ") ";
        // std::cout << "(" << end_pos.x << ", " << end_pos.y << ", " << end_pos.z << ")" << std::endl;
        if (cur_cell.pos == end_pos)
            return reconstruct_path(closed_heap, cur_cell.pos);
            
        for (int dx = -1; dx <= 1; dx++)
            for (int dz = -1; dz <= 1; dz++) {

                if (dx == 0 && dz == 0)
                    continue;

                int nx = dx + cur_cell.pos.x;
                int ny = cur_cell.pos.y;
                int nz = dz + cur_cell.pos.z;

                glm::vec3 new_pos = glm::vec3(nx, ny, nz);

                if (!grid->adjust_to_ground(new_pos, max_step_up, max_drop, max_y_diff))
                    continue;
                    
                
                OccupancyCell ground_occupancy_cell = grid->get_cell(new_pos - glm::vec3(0, 1, 0));
                if (ground_occupancy_cell.curvature >= curvature_limit)
                    continue; 

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
                
                new_cell.f = new_g + get_heuristic(new_pos, end_pos);

                pq.push(new_cell);
            }
    }
    // std::cout << "FFFFFFFFFF" << std::endl;
    return {};
}