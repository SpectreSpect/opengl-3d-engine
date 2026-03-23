#version 430 core

layout(location = 0) in vec3 aPos;

out vec3 vDir;

uniform mat4 uProj;
uniform mat4 uView;

void main() {
    vDir = aPos;

    mat4 viewRot = mat4(mat3(uView));
    vec4 pos = uProj * viewRot * vec4(aPos, 1.0);

    gl_Position = pos.xyww;
}