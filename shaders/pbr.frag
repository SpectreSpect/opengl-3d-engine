#version 450

layout(location = 0) in vec4 outColor;
layout(location = 1) in vec2 outUV;
layout(location = 2) in vec4 outPosition;


layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform sampler2D albedoTex;
layout(set = 0, binding = 2) uniform samplerCube envMap;

void main() { 
    vec3 dir = normalize(outPosition.xyz);
    vec4 c = texture(envMap, dir);
    // outFragColor = texture(albedoTex, outUV.xy);
    outFragColor = c;
}