#include "vf_program.h"

VfProgram::VfProgram(VertexShader* vertex_shader, FragmentShader* fragment_shader) 
: Program({vertex_shader, fragment_shader}) {

}