#version 330 core

in vec3 vNormal;
in vec3 vFragPos;
in vec3 vColor;

out vec4 FragColor;

uniform vec3 uViewPos;

void main() {
    vec3 light_dir = normalize(vec3(1, 1.5, 1.3));
    // vec3 albedo = vec3(0.7, 0.1, 0.1);
    vec3 albedo = vColor;
    vec3 normal = normalize(vNormal);

    // Ambient
    // vec3 ambient = vec3(0.776470588, 0.988235294, 1.0) * albedo;
    vec3 ambient = vec3(0.858823529f, 0.737254902f, 0.176470588f) * albedo;

    // Diffuse
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 diffuse = diff * albedo;

    // Specular
    vec3 view_dir = normalize(uViewPos - vFragPos);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), 70.0);
    vec3 specular = vec3(0.8958823529f, 0.837254902f, 0.276470588f) * spec;

    // Final color
    vec3 color = ambient * 0.8 + specular + diffuse;

    FragColor = vec4(color, 1.0);
}
