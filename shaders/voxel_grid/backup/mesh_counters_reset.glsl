#version 430
layout(local_size_x = 1) in;

layout(std430, binding=10) buffer MeshCounters { uvec2 meshCounters; };

void main() {
    meshCounters = uvec2(0u, 0u);
}
