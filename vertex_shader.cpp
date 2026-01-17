#include "vertex_shader.h"


VertexShader::VertexShader(std::string path) {
    std::string src = load_text_file(path);
    this->id = compile_shader(GL_VERTEX_SHADER, src.c_str());
}