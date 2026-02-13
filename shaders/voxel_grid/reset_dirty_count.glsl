#version 430
layout(local_size_x = 1) in;

layout(std430, binding=5) buffer FrameCounters { uvec4 counters; };

void main() {
    counters.y = 0u;
}
