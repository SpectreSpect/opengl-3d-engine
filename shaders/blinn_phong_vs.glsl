#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in mat4 instanceModel;

uniform mat4 uMVP;

out vec3 vFragPos;
out vec3 vNormal;

void main() {
    vFragPos = vec3(instanceModel * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(instanceModel))) * aNormal;
    gl_Position = uMVP * vec4(vFragPos, 1.0);
}
