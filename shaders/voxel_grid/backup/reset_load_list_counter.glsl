#version 430
layout(local_size_x = 1) in;

layout(std430, binding=0) buffer SteamCounters { uvec2 stream; };


void main() {
    stream.x = 0u;
}
