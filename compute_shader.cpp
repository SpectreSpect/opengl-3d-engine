#include "compute_shader.h"

ComputeShader::ComputeShader(
        const std::filesystem::path& path, 
        const std::vector<std::filesystem::path>& include_directories,
        const std::filesystem::path& debug_path
) {
    std::string src = load_text_file(path, include_directories);

    if (debug_path != "") {
        std::ofstream out(debug_path);
        out << src;
        out.close();
    }

    this->id = compile_shader(GL_COMPUTE_SHADER, src.c_str(), &path);
}