#version 330 core

layout(location = 0) in vec3 aP0;      // per-instance
layout(location = 1) in vec3 aP1;      // per-instance
layout(location = 2) in vec2 aCorner;  // per-vertex: (endpoint01, sideSign)

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2  uViewport;
uniform float uLineWidthPx;

void main() {
    // Transform endpoints into world space first
    vec4 p0w = uModel * vec4(aP0, 1.0);
    vec4 p1w = uModel * vec4(aP1, 1.0);

    // Then into clip space
    vec4 c0 = uProj * uView * p0w;
    vec4 c1 = uProj * uView * p1w;

    // NDC positions
    vec2 n0 = c0.xy / c0.w;
    vec2 n1 = c1.xy / c1.w;

    vec2 d = n1 - n0;
    float len = length(d);
    vec2 dir = (len > 1e-6) ? (d / len) : vec2(1.0, 0.0);
    vec2 perp = vec2(-dir.y, dir.x);

    // Convert pixels to NDC
    vec2 pxToNdc = vec2(2.0 / uViewport.x, 2.0 / uViewport.y);

    float halfW = 0.5 * uLineWidthPx;
    vec2 offsetNdc = perp * (aCorner.y * halfW) * pxToNdc;

    // Choose endpoint
    vec4 base = mix(c0, c1, aCorner.x);

    // Apply screen-space thickness
    base.xy += offsetNdc * base.w;

    gl_Position = base;
}