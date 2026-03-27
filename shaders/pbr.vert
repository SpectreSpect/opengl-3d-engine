#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent; // xyz=tangent, w=handedness

layout(location = 0) out vec3 vFragPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vTangent;
layout(location = 3) out float vTangentSign;
layout(location = 4) out vec3 vFragPosView;
layout(location = 5) out vec3 vColor;
layout(location = 6) out vec2 vUV;

layout(std140, set = 0, binding = 0) uniform PBRUniformBuffer {
    mat4 mvp;
    mat4 model;
    mat4 view;

    vec4 viewPos;                // xyz used
    vec4 environmentMultiplier;  // xyz used

    uvec4 clusterGrid;           // xTiles, yTiles, zSlices, maxLightsPerCluster
    vec4  screenParams;          // screenWidth, screenHeight, nearPlane, farPlane
    vec4  pbrParams;             // normalStrength, prefilterMaxMip, exposure, unused
} ubo;

void main()
{
    vec4 worldPos = ubo.model * vec4(inPosition.xyz, 1.0);

    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));

    vFragPos      = worldPos.xyz;
    vFragPosView  = vec3(ubo.view * worldPos);
    vNormal       = normalize(normalMatrix * inNormal.xyz);
    vTangent      = normalize(normalMatrix * inTangent.xyz);
    vTangentSign  = inTangent.w;
    vColor        = inColor.xyz;
    vUV           = inUV;

    gl_Position = ubo.mvp * vec4(inPosition.xyz, 1.0);
}