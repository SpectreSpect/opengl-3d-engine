#version 430 core

in vec3 vNormal;
in vec3 vFragPos;
in vec3 VFragPosView;
in vec3 vColor;

out vec4 FragColor;

struct LightSource {
    vec4 position;
    vec4 color;
};
layout(std430, binding=5) buffer LightSources { LightSource light_sources[]; };
layout(std430, binding=6) buffer NumLightsInClusters { uint num_lights_in_clusters[]; };
layout(std430, binding=7) buffer LightsInClusters { uint lights_in_clusters[]; };

uniform uint xTiles;
uniform uint yTiles;
uniform uint zSlices;
uniform float screenWidth;
uniform float screenHeight;
uniform float nearPlane;
uniform float farPlane;

uniform vec3 uViewPos;
// uniform uint max_num_light_sources;
uniform uint max_lights_per_cluster;
// uniform uint num_light_sources;




uint computeClusterID()
{
    // 1) tile X/Y from screen coords
    // gl_FragCoord.xy origin is bottom-left in GL
    float fx = clamp(gl_FragCoord.x, 0.0, screenWidth - 1.0);
    float fy = clamp(gl_FragCoord.y, 0.0, screenHeight - 1.0);
    uint tileX = uint(fx * float(xTiles) / screenWidth);
    uint tileY = uint(fy * float(yTiles) / screenHeight);
    tileX = min(tileX, xTiles - 1u);
    tileY = min(tileY, yTiles - 1u);

    // 2) view-space depth (positive forward)
    // float viewZ = -VFragPosView.z; // IMPORTANT: camera faces -Z in view space
    float viewZ = -VFragPosView.z; // IMPORTANT: camera faces -Z in view space

    // handle behind-camera / near/far
    if (viewZ <= 0.0) {
        // behind camera: put into first slice (or discard earlier)
        viewZ = nearPlane;
    }

    // 3) compute zSlice
    float sliceT;
    // if (useLogSlices) {
    float lnZ = log(max(viewZ, 1e-6));    // avoid log(0)
    float lnN = log(max(nearPlane, 1e-6));
    float lnF = log(max(farPlane, lnN + 1e-6));
    sliceT = (lnZ - lnN) / (lnF - lnN);
    // } else {
    //     sliceT = (viewZ - nearPlane) / (farPlane - nearPlane);
    // }
    sliceT = clamp(sliceT, 0.0, 0.999999); // keep <1.0 so floor works
    uint zSlice = uint(floor(sliceT * float(zSlices)));
    zSlice = min(zSlice, zSlices - 1u);

    // 4) flatten
    uint clusterID = tileX + tileY * xTiles + zSlice * xTiles * yTiles;
    return clusterID;
    // return vec3(tileX, tileY, zSlice);
}


void main() {
    
    vec3 global_light_dir = normalize(vec3(1, 1.5, 1.3));
    // vec3 global_light_color = vec3(0.15, 0.15, 0.18);
    vec3 global_light_color = vec3(0, 0, 0);
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


    uint cluster_id = computeClusterID();
    uint cluster_base = cluster_id * max_lights_per_cluster;

    // vec3 cluster_id = computeClusterID();
    // color.x = num_lights_in_clusters[cluster_id];
    // color.y = cluster_id / 10.0;
    // color.z = cluster_id.z / 10.0;


    for (int i = 0; i < num_lights_in_clusters[cluster_id]; i++) {
    // for (int i = 0; i < 1; i++) {

        LightSource light_source = light_sources[lights_in_clusters[cluster_base + i]];
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

        // color = vec3(1.0, 0, 0);
    }

    // for (int i = 0; i < num_light_sources; i++) {
    // // for (int i = 0; i < 1; i++) {
    //     LightSource light_source = light_sources[i];
    //     vec3 light_dir = normalize(light_source.position.xyz - vFragPos);
    //     float dist_to_light = distance(light_source.position.xyz, vFragPos);
    //     float light_radius = light_source.position.w;
    //     // float light_radius = 20;
    //     float light_intensity = max((light_radius - dist_to_light) / light_radius, 0.0);

    //     vec3 light_halfway_dir = normalize(light_dir + view_dir);
    //     float light_spec = pow(max(dot(normal, light_halfway_dir), 0.0), 70.0);
    //     vec3 light_specular = light_source.color.xyz * light_spec;

    //     float light_diff = max(dot(normal, light_dir), 0.0);
    //     vec3 light_diffuse = light_diff * light_source.color.xyz * albedo;

    //     color += (light_specular + light_diffuse) * light_intensity;
    // }




    // vec3 color = light_sources[0].color.xyz;
    

    FragColor = vec4(color, 1.0);
    // FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
