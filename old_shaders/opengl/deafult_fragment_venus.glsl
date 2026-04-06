#version 330 core

in vec3 vNormal;
in vec3 vFragPos;
in vec3 vColor;
in float vAO;

out vec4 FragColor;

uniform vec3 uViewPos;

void main() {
    vec3 light_dir = normalize(vec3(1, 1.5, 1.3));
    vec3 albedo = vColor;
    vec3 normal = normalize(vNormal);

// <<<<<<<< HEAD:shaders/deafult_fragment_venus.glsl
    // Ambient
    // vec3 ambient = vec3(0.776470588, 0.988235294, 1.0) * albedo;
    vec3 ambient = vec3(0.858823529f, 0.737254902f, 0.176470588f) * albedo;
// ========
    // float aoA = vAO;
    // float aoD = mix(1.0, aoA, 0.6);
    
    
    // vec3 ambientSky = vec3(0.776470588, 0.988235294, 1.0);
    // vec3 ambient = ambientSky * albedo * aoA;

    float up = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    ambient *= mix(0.6, 1.2, up);

    // Diffuse
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 diffuse = diff * albedo * aoD;

    // Specular
    vec3 view_dir = normalize(uViewPos - vFragPos);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), 70.0);
    vec3 specular = vec3(0.8958823529f, 0.837254902f, 0.276470588f) * spec;

    // vec3 color = ambient * 0.8 + specular + diffuse;
    vec3 color = ambient * 0.8 + specular + diffuse * 0.8;
    FragColor = vec4(color, 1.0);
}
