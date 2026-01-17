#pragma once
#include "vertex_shader.h"
#include "fragment_shader.h"


class Program{
public:
    GLuint id;
    Program(VertexShader* vertex_shader, FragmentShader* fragment_shader);
    ~Program();

    void use() const;
    void set_mat4(const std::string name, const glm::mat4 mat) const;
    void set_vec3(const std::string name, const glm::vec3& value) const;
};