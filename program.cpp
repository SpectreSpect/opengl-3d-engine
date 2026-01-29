#include "program.h"

Program::Program() {
    
}

Program::Program(std::vector<Shader*> shaders) {
    this->id = glCreateProgram();
    for (Shader* shader : shaders)
        glAttachShader(this->id, shader->id);

    glLinkProgram(this->id);
    GLint linkok; glGetProgramiv(this->id, GL_LINK_STATUS, &linkok);
    if (!linkok) {
        char buf[1024];
        glGetProgramInfoLog(this->id, sizeof(buf), nullptr, buf);
        std::cerr << "Program link error: " << buf << "\n";
    }
}

Program::~Program(){
    glDeleteProgram(id);
}


void Program::use() const {
    glUseProgram(id);
}

void Program::set_mat4(const std::string name, const glm::mat4 mat) const{
    this->use();
    GLint loc = glGetUniformLocation(id, name.c_str());
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found!\n";
    } else {
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
    }
}

void Program::set_vec3(const std::string name, const glm::vec3& value) const{
    this->use();
    GLint loc = glGetUniformLocation(id, name.c_str());
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found!\n";
        return;
    }
    glUniform3fv(loc, 1, &value[0]); // set the uniform
}