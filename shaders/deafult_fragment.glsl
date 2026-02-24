#version 430 core

in vec3 vNormal;
in vec3 vFragPos;
in vec3 vColor;

out vec4 FragColor;

struct LightSource {
    vec4 position;
    vec4 color;
};
layout(std430, binding=5) buffer LightSources { LightSource light_sources[]; };

uniform vec3 uViewPos;
// uniform uint max_num_light_sources;
uniform uint num_light_sources;


void main() {
    
    vec3 global_light_dir = normalize(vec3(1, 1.5, 1.3));
    vec3 global_light_color = vec3(0.15, 0.15, 0.18);
    // vec3 albedo = vec3(0.7, 0.1, 0.1);
    vec3 albedo = vColor;
    vec3 normal = normalize(vNormal);

    // Ambient
    // vec3 ambient = vec3(0.776470588, 0.988235294, 1.0) * albedo;
    vec3 ambient = global_light_color * albedo;

    // Diffuse
    float diff = max(dot(normal, global_light_dir), 0.0);
    vec3 diffuse = diff * global_light_color * albedo;

    // Specular
    vec3 view_dir = normalize(uViewPos - vFragPos);
    vec3 halfway_dir = normalize(global_light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), 70.0);
    vec3 specular = global_light_color * spec;

    // Final color
    vec3 color = (ambient + specular + diffuse);
    // vec3 color = vec3(0.0);

    for (int i = 0; i < num_light_sources; i++) {
    // for (int i = 0; i < 1; i++) {
        LightSource light_source = light_sources[i];
        vec3 light_dir = normalize(light_source.position.xyz - vFragPos);
        float dist_to_light = distance(light_source.position.xyz, vFragPos);
        float light_radius = light_source.position.w;
        // float light_radius = 20;
        float light_intensity = max((light_radius - dist_to_light) / light_radius, 0.0);

        vec3 light_halfway_dir = normalize(light_dir + view_dir);
        float light_spec = pow(max(dot(normal, light_halfway_dir), 0.0), 70.0);
        vec3 light_specular = light_source.color.xyz * light_spec;

        float light_diff = max(dot(normal, light_dir), 0.0);
        vec3 light_diffuse = light_diff * light_source.color.xyz * albedo;

        color += (light_specular + light_diffuse) * light_intensity;
    }




    // vec3 color = light_sources[0].color.xyz;
    

    FragColor = vec4(color, 1.0);
    // FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
