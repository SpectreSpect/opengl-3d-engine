#pragma once
#include "a_star.h"
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>
#include "../line.h"


class CurvatureAStar : public AStar {
public:
    PlainAstarData find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) override;
};