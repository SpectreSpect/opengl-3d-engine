#include "fragment_shader.h"

FragmentShader::FragmentShader(
    const std::filesystem::path& path, 
    const std::vector<std::filesystem::path>& include_directories
) {
    std::string src = load_text_file(path);
    this->id = compile_shader(GL_FRAGMENT_SHADER, src.c_str(), &path);
}

FragmentShader::FragmentShader(FragmentShader&& o) noexcept {
    id = o.id; 
    o.id = 0;
}

FragmentShader& FragmentShader::operator=(FragmentShader&& o) noexcept {
    if (this != &o) {
        if (id) glDeleteShader(id);
        id = o.id;
        o.id = 0;
    }
    return *this;
}