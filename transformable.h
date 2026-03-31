#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

class Transformable {
public:
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f,1.0f,1.0f};
    glm::vec3 rotation{0.0f,0.0f,0.0f};

    Transformable() = default;
    Transformable(glm::vec3 position, glm::vec3 scale, glm::vec3 rotation);

    glm::mat4 get_model_matrix() const;
};