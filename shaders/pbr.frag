#version 450

#define USE_CLUSTERED_LIGHTS 1

layout(location = 0) in vec3 vFragPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vTangent;
layout(location = 3) in float vTangentSign;
layout(location = 4) in vec3 vFragPosView;
layout(location = 5) in vec3 vColor;
layout(location = 6) in vec2 vUV;

layout(location = 0) out vec4 outFragColor;

struct LightSource {
    vec4 position; // xyz = position, w = radius
    vec4 color;    // xyz = color,    w = intensity
};

layout(std140, set = 0, binding = 0) uniform PBRUniformBuffer {
    mat4 view;
    mat4 proj;

    // mat4 model;

    vec4 viewPos;                // xyz used
    vec4 environmentMultiplier;  // xyz used

    uvec4 clusterGrid;           // xTiles, yTiles, zSlices, maxLightsPerCluster
    vec4  screenParams;          // screenWidth, screenHeight, nearPlane, farPlane
    vec4  pbrParams;             // normalStrength, prefilterMaxMip, exposure, unused
} ubo;

// layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
// layout(set = 0, binding = 2) uniform samplerCube prefilterMap;
// layout(set = 0, binding = 3) uniform sampler2D  uBrdfLUT;
layout(set = 0, binding = 1) uniform sampler2D driveaway_texture;

#if USE_CLUSTERED_LIGHTS
layout(std430, set = 0, binding = 4) readonly buffer LightSourcesBuffer {
    LightSource light_sources[];
};

layout(std430, set = 0, binding = 5) readonly buffer NumLightsInClustersBuffer {
    uint num_lights_in_clusters[];
};

layout(std430, set = 0, binding = 6) readonly buffer LightsInClustersBuffer {
    uint lights_in_clusters[];
};
#endif

const float PI = 3.14159265359;

uint computeClusterID()
{
    float screenWidth  = ubo.screenParams.x;
    float screenHeight = ubo.screenParams.y;
    float nearPlane    = ubo.screenParams.z;
    float farPlane     = ubo.screenParams.w;

    uint xTiles = ubo.clusterGrid.x;
    uint yTiles = ubo.clusterGrid.y;
    uint zSlices = ubo.clusterGrid.z;

    float fx = clamp(gl_FragCoord.x, 0.0, screenWidth - 1.0);
    float fy = clamp(gl_FragCoord.y, 0.0, screenHeight - 1.0);

    uint tileX = uint(fx * float(xTiles) / screenWidth);
    uint tileY = uint(fy * float(yTiles) / screenHeight);
    tileX = min(tileX, xTiles - 1u);
    tileY = min(tileY, yTiles - 1u);

    float viewZ = -vFragPosView.z;
    if (viewZ <= 0.0)
        viewZ = nearPlane;

    float lnZ = log(max(viewZ, 1e-6));
    float lnN = log(max(nearPlane, 1e-6));
    float lnF = log(max(farPlane, lnN + 1e-6));

    float sliceT = (lnZ - lnN) / (lnF - lnN);
    sliceT = clamp(sliceT, 0.0, 0.999999);

    uint zSlice = uint(floor(sliceT * float(zSlices)));
    zSlice = min(zSlice, zSlices - 1u);

    return tileX + tileY * xTiles + zSlice * xTiles * yTiles;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 1e-6);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggxV  = GeometrySchlickGGX(NdotV, roughness);
    float ggxL  = GeometrySchlickGGX(NdotL, roughness);
    return ggxV * ggxL;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void accumulateLight(
    in vec3 N,
    in vec3 V,
    in vec3 albedo,
    in float metallic,
    in float roughness,
    in vec3 F0,
    in vec3 L,
    in vec3 radiance,
    inout vec3 Lo)
{
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    vec3 numerator    = NDF * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 1e-6);
    vec3 specular     = numerator / denominator;

    Lo += (kD * albedo / PI + specular) * radiance * NdotL;
}

void main()
{
    vec3 albedo = clamp(vColor, 0.0, 1.0);
    float metallic = 0.0;
    float roughness = 1.0;
    float ao = 1.0;

    roughness = clamp(roughness, 0.0005, 1.0);
    metallic  = clamp(metallic, 0.0, 1.0);
    ao        = clamp(ao, 0.0, 1.0);

    vec3 N = normalize(vNormal);
    vec3 V = normalize(ubo.viewPos.xyz - vFragPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    vec3 global_light_dir   = normalize(vec3(1.0, 1.5, 1.3));
    vec3 global_light_color = vec3(0.0);

//     accumulateLight(
//         N, V, albedo, metallic, roughness, F0,
//         global_light_dir,
//         global_light_color,
//         Lo
//     );

// #if USE_CLUSTERED_LIGHTS
//     uint max_lights_per_cluster = ubo.clusterGrid.w;

//     uint cluster_id   = computeClusterID();
//     uint cluster_base = cluster_id * max_lights_per_cluster;

//     uint count = min(num_lights_in_clusters[cluster_id], max_lights_per_cluster);

//     for (uint i = 0u; i < count; ++i) {
//         LightSource light_source = light_sources[lights_in_clusters[cluster_base + i]];

//         vec3 light_vec = light_source.position.xyz - vFragPos;
//         float dist_to_light = length(light_vec);
//         vec3 L = light_vec / max(dist_to_light, 1e-6);

//         float radius = light_source.position.w;
//         float x = clamp(dist_to_light / radius, 0.0, 1.0);
//         float fade = 1.0 - smoothstep(0.0, 1.0, x);
//         float attenuation = fade / max(dist_to_light * dist_to_light, 1e-4);

//         vec3 radiance = light_source.color.xyz *
//                         light_source.color.w *
//                         attenuation *
//                         (radius * radius);

//         accumulateLight(
//             N, V, albedo, metallic, roughness, F0,
//             L,
//             radiance,
//             Lo
//         );
//     }
// #endif

//     float NdotV = max(dot(N, V), 0.0);

//     vec3 F  = fresnelSchlickRoughness(NdotV, F0, roughness);
//     vec3 kS = F;
//     vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

//     vec3 irradiance = texture(irradianceMap, N).rgb * ubo.environmentMultiplier.xyz;
//     vec3 diffuseIBL = irradiance * albedo;

//     float lod = roughness * min(ubo.pbrParams.y, 4.0);
//     vec3 prefilteredColor = textureLod(
//         prefilterMap,
//         normalize(R),
//         lod
//     ).rgb * ubo.environmentMultiplier.xyz;

//     // vec3 prefilteredColor = vec3(1.0, 1.0, 1.0) * ubo.environmentMultiplier.xyz;

//     vec2 brdf = texture(uBrdfLUT, vec2(NdotV, roughness)).rg;
//     vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

//     vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;

//     vec3 color = ambient + Lo;

//     float exposure = ubo.pbrParams.z;
//     color *= exposure;

//     color = color / (color + vec3(1.0));
//     // color = pow(color, vec3(1.0 / 2.2));

    outFragColor = vec4(texture(driveaway_texture, vUV).rgb, 1.0);

    // outFragColor = vec4(ubo.pbrParams.y, ubo.pbrParams.y, ubo.pbrParams.y, 1.0);

    
    
    // outFragColor = vec4(ubo.environmentMultiplier.xyz, 1.0);
}