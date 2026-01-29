#pragma once
#include "shader.h"


class Program{
public:
    GLuint id;
    Program();
    Program(std::vector<Shader*> shaders);
    ~Program();

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&& o) noexcept : id(o.id) { o.id = 0; }
    Program& operator=(Program&& o) noexcept {
        if (this != &o) {
            if (id) glDeleteProgram(id);
            id = o.id;
            o.id = 0;
        }
        return *this;
    }

    void use() const;
    void set_mat4(const std::string name, const glm::mat4 mat) const;
    void set_vec3(const std::string name, const glm::vec3& value) const;
};