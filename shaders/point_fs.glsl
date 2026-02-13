#version 330 core
out vec4 FragColor;

in vec2 vLocal; // [-1..+1]

uniform vec4 uColor;
uniform int  uRound; // 1 = circle, 0 = square

void main() {
    vec4 col = uColor;

    if (uRound == 1) {
        // distance from center, radius = 1 at quad edge
        float r = length(vLocal);

        // smooth edge anti-aliasing (no harsh discard)
        float aa = fwidth(r);
        float alpha = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);

        col.a *= alpha;

        // optional: avoid drawing fully transparent fragments
        if (col.a <= 0.001) discard;
    }

    FragColor = col;
}
