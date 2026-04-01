#version 450

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 FragColor;

void main() {
    vec4 col = vColor;

    float r = length(vLocal);
    float aa = fwidth(r);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);
    col.a *= alpha;

    if (col.a <= 0.001) {
        discard;
    }

    vec3 color = pow(col.xyz, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}