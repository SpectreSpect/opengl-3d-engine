#include "compute_shader.h"

ComputeShader::ComputeShader(std::string& path) {
    std::string src = load_text_file(path);
    this->id = compile_shader(GL_COMPUTE_SHADER, src.c_str());
}