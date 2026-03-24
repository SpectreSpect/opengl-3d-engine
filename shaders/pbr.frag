#version 450

layout(location = 0) in vec4 outColor;
layout(location = 1) in vec2 outUV;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform sampler2D albedoTex;

void main() { 
    outFragColor = texture(albedoTex, outUV.xy);
}