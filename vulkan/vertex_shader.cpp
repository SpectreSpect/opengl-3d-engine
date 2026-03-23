#include "vertex_shader.h"


VertexShader::VertexShader(
    const std::filesystem::path& path, 
    const std::vector<std::filesystem::path>& include_directories
) {
    std::string src = load_text_file(path, include_directories);
    this->id = compile_shader(GL_VERTEX_SHADER, src.c_str(), &path);
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