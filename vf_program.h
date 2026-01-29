#pragma once
#include "program.h"
#include "vertex_shader.h"
#include "fragment_shader.h"

class VfProgram : public Program {
public:
    VfProgram(VertexShader* vertex_shader, FragmentShader* fragment_shader);
};