#pragma once
#include "a_star.h"

struct NonholonomicPos {
    glm::vec3 pos;
    glm::vec3 theta; // orientation
};

class NonholonomicAStar : public AStar {
public:
    float wheel_base = 2.5f;
    float max_steer = 0.6;
    float integration_steps = 8;
    float motion_simulation_dist = 3.0f;

    std::vector<NonholonomicPos> simulate_motion(NonholonomicPos start_pos, int steer);
    std::vector<glm::ivec3> find_nonholomic_path(NonholonomicPos start_pos, NonholonomicPos end_pos);
};