#pragma once
#include "occupancy_grid_3d.h"
#include <queue>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include "../voxel_engine/voxel_grid.h"
#include "voxel_occupancy_grid_3d.h"

struct AStarCell {
    float g;
    float f;
    glm::ivec3 pos;
    glm::ivec3 came_from;
    bool no_parent = true;
};

struct ByPriority {
    bool operator()(const AStarCell& a, const AStarCell& b) const {
        return a.f > b.f; // higher priority first
    }
};


class AStar {
public:
    OccupancyGrid3D* grid;
    

    AStar();
    AStar(VoxelGrid* voxel_grid);

    float get_heuristic(glm::ivec3 a, glm::ivec3 b) {
        glm::vec3 d = glm::vec3(a - b);
        return glm::dot(d, d); // squared distance
    }

    std::vector<glm::ivec3> reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos) {
        std::vector<glm::ivec3> path;
        path.push_back(pos);
        glm::ivec3 cur_pos = pos;

        while (true) {
            uint64_t cur_key = grid->pack_key(cur_pos.x, cur_pos.y, cur_pos.z);
            auto it = closed_heap.find(cur_key);

            if (it == closed_heap.end())
                return {};
                
            
            AStarCell prev_cell = it->second;

            // std::cout << prev_cell.f << std::endl;

            if (prev_cell.no_parent)
                break;
            
            
            path.push_back(cur_pos);
            cur_pos = prev_cell.came_from;
        }

        std::reverse(path.begin(), path.end());

        return path;
    }

    std::vector<glm::ivec3> find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) {
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

            // std::cout << "(" << cur_cell.pos.x << ", " << cur_cell.pos.y << ", " << cur_cell.pos.z << ")" << std::endl;

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

                    // bool ascended = false;
                    // int not_ascend_penalty = 0;

                    if (grid->get_cell(new_pos).solid) {
                        if (grid->get_cell(new_pos + glm::ivec3(0, 1, 0)).solid)
                            continue;
                        new_pos.y += 1;
                        // ascended = true;
                    }
                    else {
                        if (!grid->get_cell(new_pos + glm::ivec3(0, -1, 0)).solid)
                            if (!grid->get_cell(new_pos + glm::ivec3(0, -2, 0)).solid)
                                continue;
                            else {
                                new_pos.y -= 1;
                                // ascended = true;
                            }
                    }

                    // if (!ascended)
                    //     not_ascend_penalty = 10000;

                    

                        


                    // OccupancyCell cell_1 = grid->get_cell(new_pos);

                    // int ascend_penalty = 0;
                    
                    // if (cell_1.solid) {
                    //     new_pos.y += 1;

                    //     OccupancyCell cell_2 = grid->get_cell(new_pos);

                    //     ascend_penalty = 0.5;
                        
                    //     if (cell_2.solid)
                    //         continue;
                    // }


                    // OccupancyCell cell_3 = grid->get_cell(new_pos - glm::ivec3(0, 1, 0));

                    // if (!cell_3.solid) {
                    //     new_pos.y - 1;

                    //     OccupancyCell cell_4 = grid->get_cell(new_pos - glm::ivec3(0, 1, 0));

                    //     if (!cell_4.solid)
                    //         continue;
                    // }
                        

                    uint64_t new_key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
                    auto heap_it = closed_heap.find(new_key);
                    if (heap_it != closed_heap.end())
                        continue;
                    

                    float new_g = cur_cell.g + glm::distance((glm::vec3)cur_cell.pos, (glm::vec3)new_pos);
                    // float new_g = cur_cell.g + 1;

                    uint64_t key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
                    auto it = g_score.find(key);
                    
                    if (it != g_score.end()) {
                        float old_g = it->second;
                        if (old_g <= new_g)
                            continue;
                    }

                    g_score[key] = new_g;

                    // float new_f = new_g + get_heuristic(new_pos, end_pos);

                    AStarCell new_cell;
                    new_cell.pos = new_pos;
                    new_cell.came_from = cur_cell.pos;
                    new_cell.no_parent = false;
                    new_cell.g = new_g;
                    // new_cell.f = new_g + get_heuristic(new_pos, end_pos) + not_ascend_penalty;
                    new_cell.f = new_g + get_heuristic(new_pos, end_pos);


                    
                    pq.push(new_cell);
                }
        }

        return {};
    }
};