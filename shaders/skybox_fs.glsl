#version 430 core

in vec3 vDir;
out vec4 FragColor;

uniform samplerCube uSkybox;
uniform float uExposure = 0.2;

void main() {
    vec3 hdrColor = texture(uSkybox, normalize(vDir)).rgb;

    // exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * uExposure);

    // gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(mapped, 1.0);
}