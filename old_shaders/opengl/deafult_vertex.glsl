#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;
layout(location = 4) in vec4 aTangent; // xyz=tangent, w=handedness

out vec3 vFragPos;
out vec3 vNormal;
out vec3 vTangent;
out float vTangentSign;
out vec3 VFragPosView;
out vec3 vColor;
out vec2 vUV;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uView;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(uModel)));

    vFragPos = worldPos.xyz;
    VFragPosView = vec3(uView * worldPos);
    vNormal = normalize(normalMatrix * aNormal);
    vTangent = normalize(normalMatrix * aTangent.xyz);
    vTangentSign = aTangent.w;
    vColor = aColor;
    vUV = aUV;

    gl_Position = uMVP * vec4(aPos, 1.0);
}