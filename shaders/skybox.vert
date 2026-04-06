#version 460

layout(location = 0) in vec4 inPosition;

layout(location = 0) out vec4 outDir;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    float exposure;
} ubo;

void main() {
    outDir = inPosition;

    mat4 viewRot = mat4(mat3(ubo.view));
    vec4 pos = ubo.proj * viewRot * vec4(inPosition.xyz, 1.0);

    gl_Position = pos.xyww;
}


// #version 430 core

// layout(location = 0) in vec3 aPos;

// out vec3 vDir;

// uniform mat4 uProj;
// uniform mat4 uView;

// void main() {
//     vDir = aPos;

//     mat4 viewRot = mat4(mat3(uView));
//     vec4 pos = uProj * viewRot * vec4(aPos, 1.0);

//     gl_Position = pos.xyww;
// }