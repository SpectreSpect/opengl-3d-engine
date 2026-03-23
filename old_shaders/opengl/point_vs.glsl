#version 330 core

layout(location = 0) in vec3 aPos;      
layout(location = 1) in vec2 aCorner;   
layout(location = 2) in vec3 aColor;    // per-instance

uniform mat4 uModel;   // NEW
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2  uViewport;     
uniform float uPointSizePx; 
uniform float uPointSizeWorld;
uniform int   uScreenSpaceSize; // 1 = pixels, 0 = world

out vec2 vLocal;
out vec3 vColor;

void main() {
    vLocal = aCorner;
    vColor = aColor;

    // Transform point center into world space
    vec4 centerW4 = uModel * vec4(aPos, 1.0);

    if (uScreenSpaceSize == 1) {
        // --- screen-space (constant pixel size) ---
        vec4 c = uProj * uView * centerW4;

        vec2 pxToNdc = vec2(2.0 / uViewport.x, 2.0 / uViewport.y);
        float radiusPx = 0.5 * uPointSizePx;
        vec2 offsetNdc = aCorner * radiusPx * pxToNdc;

        c.xy += offsetNdc * c.w;
        gl_Position = c;
    } else {
        // --- world-space (scales with distance naturally) ---

        // Camera basis in world space from view matrix rows
        vec3 rightW = vec3(uView[0][0], uView[1][0], uView[2][0]);
        vec3 upW    = vec3(uView[0][1], uView[1][1], uView[2][1]);

        float radiusW = 0.5 * uPointSizeWorld;

        vec3 centerW = centerW4.xyz;
        vec3 posW = centerW
                  + rightW * (aCorner.x * radiusW)
                  + upW    * (aCorner.y * radiusW);

        gl_Position = uProj * uView * vec4(posW, 1.0);
    }
}
