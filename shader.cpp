#include "shader.h"

Shader::~Shader() {
    glDeleteShader(id);
}

void Shader::print_shader_log(const char* name) {
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);

    GLint len = 0;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);

    std::string log;
    if (len > 1) {
        log.resize(len);
        glGetShaderInfoLog(id, len, nullptr, log.data());
    }

    std::cout << "[COMPILE] " << name
              << " shader=" << id
              << " ok=" << ok << "\n";
    if (!log.empty()) std::cout << log << "\n";
}

std::string Shader::load_text_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
    }
    return s;
}
