#pragma once
#include "shader.h"

class ComputeShader : public Shader {
public:
    ComputeShader(std::string& path);

    ComputeShader(const ComputeShader&) = delete;
    ComputeShader& operator=(const ComputeShader&) = delete;
    ComputeShader(ComputeShader&& o) noexcept { id = o.id; o.id = 0; }
    ComputeShader& operator=(ComputeShader&& o) noexcept {
        if (this != &o) {
            if (id) glDeleteShader(id);
            id = o.id;
            o.id = 0;
        }
        return *this;
    }

};