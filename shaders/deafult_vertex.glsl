#version 430 core

layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aNormal;
layout(location = 2) in vec4 aColor;

out vec3 vNormal;
out vec3 vFragPos;
out vec3 VFragPosView;
out vec3 vColor;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uView;

void main() {
    vFragPos = vec3(uModel * vec4(vec3(aPos), 1.0));
    VFragPosView = vec3(uView * vec4(vec3(vFragPos), 1.0));
    // vColor = aColor;
    vNormal = mat3(transpose(inverse(uModel))) * vec3(aNormal);
    vColor = vec3(aColor);

    gl_Position = uMVP * vec4(vec3(aPos), 1.0);
}
