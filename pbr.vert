#version 430 core


layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aNormal;
layout(location = 2) in vec4 aColor;
// layout(location = 3) in vec2 aUV;
// layout(location = 4) in vec4 aTangent; // xyz=tangent, w=handedness

out vec3 vFragPos;
out vec3 vNormal;
// out vec3 vTangent;
// out float vTangentSign;
out vec3 VFragPosView;
out vec3 vColor;
// out vec2 vUV;

// uniform mat4 uMVP;
// uniform mat4 uModel;
// uniform mat4 uView;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 uMVP;
    mat4 uModel;
    mat4 uView;
} ubo;

void main()
{
    vec4 worldPos = ubo.uModel * vec4(aPos, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(ubo.uModel)));

    vFragPos = worldPos.xyz;
    VFragPosView = vec3(ubo.uView * worldPos);
    vNormal = normalize(normalMatrix * aNormal);
    // vTangent = normalize(normalMatrix * aTangent.xyz);
    // vTangentSign = aTangent.w;
    vColor = aColor;
    // vUV = aUV;

    gl_Position = ubo.uMVP * vec4(aPos, 1.0);
}