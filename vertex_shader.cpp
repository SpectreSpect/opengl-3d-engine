#include "vertex_shader.h"


VertexShader::VertexShader(std::string path) {
    std::string src = load_text_file(path);
    this->id = compile_shader(GL_VERTEX_SHADER, src.c_str());
}

VertexShader::VertexShader(VertexShader&& o) noexcept {
    id = o.id;
    o.id = 0;
}

VertexShader& VertexShader::operator=(VertexShader&& o) noexcept {
    if (this != &o) {
        if (id) glDeleteShader(id);
        id = o.id;
        o.id = 0;
    }
    return *this;
}