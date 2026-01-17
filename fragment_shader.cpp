#include "fragment_shader.h"


FragmentShader::FragmentShader(std::string path) {
    std::string src = load_text_file(path);
    this->id = compile_shader(GL_FRAGMENT_SHADER, src.c_str());
}