#version 430 core

in vec3 vNormal;
in vec3 vFragPos;
in vec3 VFragPosView;
in vec3 vColor;
in vec2 vUV;
in vec3 vTangent;
in float vTangentSign;

out vec4 FragColor;

struct LightSource {
    vec4 position; // xyz = position, w = radius
    vec4 color;    // xyz = color,    w = intensity
};

layout(std430, binding=5) buffer LightSources { LightSource light_sources[]; };
layout(std430, binding=6) buffer NumLightsInClusters { uint num_lights_in_clusters[]; };
layout(std430, binding=7) buffer LightsInClusters { uint lights_in_clusters[]; };

uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D  uBrdfLUT;
uniform sampler2D  uAlbedo;
uniform sampler2D  uNormal;
uniform sampler2D  uORM;

uniform int   uUseAlbedoTexture = 1;
uniform int   uUseNormalTexture = 1;
uniform int   uUseORMTexture    = 1;

uniform float uNormalStrength   = 1.0;
uniform float uPrefilterMaxMip;

uniform uint xTiles;
uniform uint yTiles;
uniform uint zSlices;
uniform float screenWidth;
uniform float screenHeight;
uniform float nearPlane;
uniform float farPlane;

uniform vec3 uViewPos;
uniform uint max_lights_per_cluster;

const float PI = 3.14159265359;

uint computeClusterID()
{
    float fx = clamp(gl_FragCoord.x, 0.0, screenWidth - 1.0);
    float fy = clamp(gl_FragCoord.y, 0.0, screenHeight - 1.0);

    uint tileX = uint(fx * float(xTiles) / screenWidth);
    uint tileY = uint(fy * float(yTiles) / screenHeight);
    tileX = min(tileX, xTiles - 1u);
    tileY = min(tileY, yTiles - 1u);

    float viewZ = -VFragPosView.z;
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

mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);

    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invMax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invMax, B * invMax, N);
}

vec3 getShadingNormal()
{
    vec3 N = normalize(vNormal);

    if (uUseNormalTexture == 0)
        return N;

    vec3 T = normalize(vTangent);
    vec3 B = normalize(cross(N, T)) * vTangentSign;

    mat3 TBN = mat3(T, B, N);

    vec3 tangentNormal = texture(uNormal, vUV).xyz * 2.0 - 1.0;

    // For *_nor_gl_* keep this as-is.
    // If it ever looks inverted, then try:
    // tangentNormal.y = -tangentNormal.y;

    tangentNormal.xy *= uNormalStrength;
    tangentNormal = normalize(tangentNormal);

    return normalize(TBN * tangentNormal);
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
    vec3 vertexAlbedo = clamp(vColor, 0.0, 1.0);
    vec3 texAlbedo    = texture(uAlbedo, vUV).rgb;
    vec3 albedo       = (uUseAlbedoTexture != 0) ? (vertexAlbedo * texAlbedo) : vertexAlbedo;

    float metallic  = 0.0;
    float roughness = 1.0;
    float ao        = 1.0;

    // Leave this off unless your uORM really is packed as:
    // R = AO, G = roughness, B = metallic
    if (uUseORMTexture != 0) {
        vec3 orm = texture(uORM, vUV).rgb;
        ao        = orm.r;
        roughness = orm.g;
        metallic  = orm.b;
    }

    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic, 0.0, 1.0);
    ao        = clamp(ao, 0.0, 1.0);

    vec3 N = getShadingNormal();
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    vec3 global_light_dir   = normalize(vec3(1.0, 1.5, 1.3));
    vec3 global_light_color = vec3(0.15);

    accumulateLight(
        N, V, albedo, metallic, roughness, F0,
        global_light_dir,
        global_light_color,
        Lo
    );

    uint cluster_id   = computeClusterID();
    uint cluster_base = cluster_id * max_lights_per_cluster;

    for (uint i = 0u; i < num_lights_in_clusters[cluster_id]; ++i) {
        LightSource light_source = light_sources[lights_in_clusters[cluster_base + i]];

        vec3 light_vec = light_source.position.xyz - vFragPos;
        float dist_to_light = length(light_vec);
        vec3 L = light_vec / max(dist_to_light, 1e-6);

        float radius = light_source.position.w;
        float x = clamp(dist_to_light / radius, 0.0, 1.0);
        float fade = 1.0 - smoothstep(0.0, 1.0, x);
        float attenuation = fade / max(dist_to_light * dist_to_light, 1e-4);

        vec3 radiance = light_source.color.xyz *
                        light_source.color.w *
                        attenuation *
                        (radius * radius);

        accumulateLight(
            N, V, albedo, metallic, roughness, F0,
            L,
            radiance,
            Lo
        );
    }

    float NdotV = max(dot(N, V), 0.0);

    vec3 F  = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    vec3 prefilteredColor = textureLod(
        prefilterMap,
        normalize(R),
        roughness * uPrefilterMaxMip
    ).rgb;

    vec2 brdf = texture(uBrdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;

    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}