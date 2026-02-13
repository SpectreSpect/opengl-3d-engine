#version 330 core

layout(location = 0) in vec3 aPos;      // per-instance
layout(location = 1) in vec2 aCorner;   // per-vertex in [-1..+1]

uniform mat4 uView;
uniform mat4 uProj;
uniform vec2 uViewport;     // (width, height) in pixels
uniform float uPointSizePx; // diameter in pixels

out vec2 vLocal; // [-1..+1] for circle mask

void main() {
    vec4 c = uProj * uView * vec4(aPos, 1.0);

    // Convert pixels to NDC
    vec2 pxToNdc = vec2(2.0 / uViewport.x, 2.0 / uViewport.y);

    // aCorner is [-1..1], so multiply by radius in pixels
    float radiusPx = 0.5 * uPointSizePx;
    vec2 offsetNdc = aCorner * radiusPx * pxToNdc;

    // apply in clip space
    c.xy += offsetNdc * c.w;

    vLocal = aCorner;
    gl_Position = c;
}
