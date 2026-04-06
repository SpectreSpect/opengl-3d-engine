#version 330 core
out vec4 FragColor;

in vec2 vLocal;
in vec3 vColor;

uniform int  uRound; // 1 = circle, 0 = square

void main() {
    vec4 col = vec4(vColor, 1.0);

    if (uRound == 1) {
        float r = length(vLocal);
        float aa = fwidth(r);
        float alpha = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);
        col.a *= alpha;
        if (col.a <= 0.001) discard;
    }

    FragColor = col;
}
