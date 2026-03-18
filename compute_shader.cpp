#include "compute_shader.h"

ComputeShader::ComputeShader(
        const std::filesystem::path& path, 
        const std::vector<std::filesystem::path>& include_directories
) {
    std::string src = load_text_file(path, include_directories);
    this->id = compile_shader(GL_COMPUTE_SHADER, src.c_str(), &path);
}