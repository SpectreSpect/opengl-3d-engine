#version 430 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(location = 3) in vec4 aInstancePosRadius; // xyz = center, w = radius
layout(location = 4) in vec4 aInstanceNormal;    // xyz = normal
layout(location = 5) in vec4 aInstanceColor;

out vec3 vNormal;
out vec3 vFragPos;
out vec3 VFragPosView;
out vec3 vColor;

// uniform mat4 uModel;
// uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;
uniform mat4 uView;

void main() {
    vec3 n = normalize(aInstanceNormal.xyz);

    // Pick any vector that is not parallel to n
    vec3 helper = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0)
                                   : vec3(1.0, 0.0, 0.0);

    // Build orthonormal basis
    vec3 tangent   = normalize(cross(helper, n));
    vec3 bitangent = cross(n, tangent);

    float radius = aInstancePosRadius.w;

    // Your base disk is in XZ plane and faces +Y
    vec3 localOffset =
          aPos.x * tangent * radius
        + aPos.z * bitangent * radius
        + aPos.y * n * radius;

    vec3 localPos = aInstancePosRadius.xyz + localOffset;
    vec4 worldPos = uModel * vec4(localPos, 1.0);

    // Rotate the vertex normal from base (+Y) space into instance space
    vec3 localNormal =
          aNormal.x * tangent
        + aNormal.z * bitangent
        + aNormal.y * n;

    vFragPos = worldPos.xyz;
    VFragPosView = vec3(uView * worldPos);
    vNormal = normalize(mat3(transpose(inverse(uModel))) * localNormal);
    vColor = aColor * aInstanceColor.xyz;

    gl_Position = uProj * uView * worldPos;
}