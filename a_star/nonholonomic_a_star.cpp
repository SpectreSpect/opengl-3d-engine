#include "nonholonomic_a_star.h"


NonholonomicAStar::NonholonomicAStar(VoxelGrid* voxel_grid) {
    this->grid = new VoxelOccupancyGrid3D(voxel_grid);
}

std::vector<NonholonomicPos> NonholonomicAStar::simulate_motion(NonholonomicPos start, int steer, int direction)
{
    steer = std::clamp(steer, -1, 1);
    direction = (direction < 0) ? -1 : 1; // only -1 or +1

    // steer = -1 => max LEFT, steer = +1 => max RIGHT
    float delta = 0.0f;
    if (steer != 0) delta = -steer * max_steer;

    std::vector<NonholonomicPos> out;
    out.reserve(integration_steps);

    glm::vec3 p = start.pos;
    float yaw   = start.theta;

    const float ds = motion_simulation_dist / float(integration_steps);
    const float ds_signed = ds * float(direction);
    const float eps = 1e-6f;

    for (int i = 0; i < integration_steps; ++i) {
        if (std::abs(delta) < eps) {
            // straight (forward or reverse)
            p.x += ds_signed * std::cos(yaw);
            p.z += ds_signed * std::sin(yaw);
        } else {
            // exact circular arc integration (forward or reverse)
            float tanD = std::tan(delta);
            // (tanD won't be ~0 here because of the eps check above)
            float R = wheel_base / tanD;   // signed radius
            float yaw0 = yaw;
            float dYaw = ds_signed / R;    // signed
            yaw += dYaw;

            p.x += R * (std::sin(yaw) - std::sin(yaw0));
            p.z += R * (-std::cos(yaw) + std::cos(yaw0));
        }

        NonholonomicPos s = start;
        s.pos = p;
        s.theta = yaw;
        out.push_back(s);
    }

    return out;
}

bool NonholonomicAStar::almost_equal(NonholonomicPos a, NonholonomicPos b) {
    float dist = glm::distance(a.pos, b.pos);
    float theta_diff = angle_diff(a.theta, a.theta);

    bool almost_equal_pos = dist <= 2.0f;
    bool almost_equal_angle = theta_diff <= 0.7f;

    return almost_equal_pos && almost_equal_angle;
}

std::vector<NonholonomicPos> NonholonomicAStar::reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos) {
    std::vector<NonholonomicPos> path;
    path.push_back(pos);
    NonholonomicPos cur_pos = pos;

    while (true) {
        uint64_t cur_key = NonholonomicKeyPacker::pack(cur_pos);
        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        NonholonomicAStarCell prev_cell = it->second;

        if (prev_cell.no_parent)
            break;
        
        path.push_back(cur_pos);
        cur_pos = prev_cell.came_from;
    }

    std::reverse(path.begin(), path.end());

    return path;
}


std::vector<NonholonomicPos> NonholonomicAStar::find_nonholomic_path(NonholonomicPos start_pos, NonholonomicPos end_pos) {
    std::priority_queue<NonholonomicAStarCell, std::vector<NonholonomicAStarCell>, NonholonomicByPriority> pq;
    std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap;
    std::unordered_map<uint64_t, float> g_score;

    NonholonomicAStarCell start;
    start.pos = start_pos;
    start.no_parent = true;
    start.g = 0;
    start.f = 0;

    int limit = 10000;
    int counter = 0;

    pq.push(start);

    while (!pq.empty()) {
        NonholonomicAStarCell cur_cell = pq.top();
        pq.pop();

        if (counter >= limit) {
            std::cout << "Limit exceeded" << std::endl;
            return {};
        }
        // std::cout << "KEK" << std::endl;

        // std::cout << "(" << cur_cell.pos.pos.x << ", " << cur_cell.pos.pos.y << ", " << cur_cell.pos.pos.z << ", " << cur_cell.pos.theta  << ")" << std::endl;
            

        counter++;
        uint64_t cur_key = NonholonomicKeyPacker::pack(cur_cell.pos);
        closed_heap[cur_key] = cur_cell;

        // if (cur_cell.pos == end_pos) {
        if (almost_equal(cur_cell.pos, end_pos)) {
            return reconstruct_path(closed_heap, cur_cell.pos);
        }
            
        for (int dir = -1; dir <= 1; dir += 2)
            for (int steer = -1; steer <= 1; steer++) {
                std::vector<NonholonomicPos> motion = NonholonomicAStar::simulate_motion(cur_cell.pos, steer, dir);

                NonholonomicPos new_pos = motion[motion.size() - 1];

                bool need_continue = false;
                float last_y = new_pos.pos.y;
                for (int i = 0; i < motion.size(); i++) {
                    NonholonomicPos intermidiate_pos = motion[i];
                    glm::vec3 vecpos = glm::ivec3(glm::floor(intermidiate_pos.pos));
                    intermidiate_pos.pos.y = last_y;
                    // print_vec(vecpos);
                    // std::cout << grid->get_cell(vecpos).solid << std::endl;

                    if (grid->get_cell(vecpos).solid) {
                        if (grid->get_cell(vecpos + glm::vec3(0, 1, 0)).solid) {
                            need_continue = true;
                            // std::cout << "A" << std::endl;
                            break;
                            // std::cout << "A" << std::endl;
                            // continue;
                        }        
                    last_y += 1;
                    // std::cout << "B" << std::endl;
                    }
                    else {
                        // print_vec(vecpos + glm::vec3(0, -1, 0));
                        // std::cout << !grid->get_cell(vecpos + glm::vec3(0, -1, 0)).solid << std::endl;
                        if (!grid->get_cell(vecpos + glm::vec3(0, -1, 0)).solid)
                            if (!grid->get_cell(vecpos + glm::vec3(0, -2, 0)).solid) {
                                // std::cout << "B" << std::endl;
                                
                                // std::cout << "C" << std::endl;
                                // std::cout << "(" << intermidiate_pos.pos.x << ", " << intermidiate_pos.pos.y << ", " << intermidiate_pos.pos.z << ", " << intermidiate_pos.theta  << ")" << std::endl;
                                // std::cout << "(" << intermidiate_pos.pos.x << ", " << intermidiate_pos.pos.y << ", " << intermidiate_pos.pos.z << ", " << intermidiate_pos.theta  << ")" << std::endl;
                                need_continue = true;
                                break;
                                // continue;
                            }          
                            else {
                                last_y -= 1;
                                // std::cout << "D" << std::endl;
                            }
                    }
                }

                if (need_continue) {
                    
                    continue;
                }

                
                    

                // if (dx == 0 && dz == 0)
                //     continue;

                // int nx = dx + cur_cell.pos.x;
                // int ny = cur_cell.pos.y;
                // int nz = dz + cur_cell.pos.z;

                // glm::ivec3 new_pos = glm::ivec3(nx, ny, nz);

                if (grid->get_cell(new_pos.pos).solid) {
                    if (grid->get_cell(new_pos.pos + glm::vec3(0, 1, 0)).solid) {
                        // std::cout << "A" << std::endl;
                        continue;
                    }
                        
                    new_pos.pos.y += 1;
                }
                else {
                    if (!grid->get_cell(new_pos.pos + glm::vec3(0, -1, 0)).solid)
                        if (!grid->get_cell(new_pos.pos + glm::vec3(0, -2, 0)).solid) {
                            // std::cout << "B" << std::endl;
                            continue;
                        }
                            
                        else {
                            new_pos.pos.y -= 1;
                        }
                }

                uint64_t new_key = NonholonomicKeyPacker::pack(new_pos);
                auto heap_it = closed_heap.find(new_key);
                if (heap_it != closed_heap.end()) 
                    continue;
                

                float new_g = cur_cell.g + glm::distance((glm::vec3)cur_cell.pos.pos, (glm::vec3)new_pos.pos);

                // uint64_t key = grid->pack_key(new_pos.x, new_pos.y, new_pos.z);
                auto it = g_score.find(new_key);
                
                if (it != g_score.end()) {
                    float old_g = it->second;
                    if (old_g <= new_g)
                        continue;
                }

                g_score[new_key] = new_g;

                NonholonomicAStarCell new_cell;
                new_cell.pos = new_pos;
                new_cell.pos.steer = steer;
                new_cell.pos.dir = dir;
                // new_cell.pos.motion = motion;
                new_cell.came_from = cur_cell.pos;
                new_cell.no_parent = false;
                new_cell.g = new_g;
                new_cell.f = new_g + get_heuristic(new_pos.pos, end_pos.pos);
       
                // std::cout << "(" << new_cell.pos.pos.x << ", " << new_cell.pos.pos.y << ", " << new_cell.pos.pos.z << ", " << new_cell.pos.theta  << ")" << std::endl;
                pq.push(new_cell);
            }
    }
    return {};
}