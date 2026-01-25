#include "program.h"

Program::Program(VertexShader* vertex_shader, FragmentShader* fragment_shader) {
    // compile shaders and program
    // GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    // GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    this->id = glCreateProgram();
    glAttachShader(this->id, vertex_shader->id);
    glAttachShader(this->id, fragment_shader->id);
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

void Program::set_float(const std::string& name, float value) const {
    this->use();
    GLint loc = glGetUniformLocation(id, name.c_str());
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found!\n";
        return;
    }
    glUniform1f(loc, value);
}

void Program::set_int(const std::string& name, int value) const {
    this->use();
    GLint loc = glGetUniformLocation(id, name.c_str());
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found!\n";
        return;
    }
    glUniform1i(loc, value);
}

void Program::set_vec2(const std::string& name, const glm::vec2& value) const {
    this->use();
    GLint loc = glGetUniformLocation(id, name.c_str());
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found!\n";
        return;
    }
    glUniform2fv(loc, 1, &value[0]);
}

void Program::set_vec4(const std::string& name, const glm::vec4& value) const {
    this->use();
    GLint loc = glGetUniformLocation(id, name.c_str());
    if (loc == -1) {
        std::cerr << "Warning: uniform '" << name << "' not found!\n";
        return;
    }
    glUniform4fv(loc, 1, &value[0]);
}
