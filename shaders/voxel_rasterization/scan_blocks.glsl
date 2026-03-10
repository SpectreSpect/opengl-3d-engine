#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer In   { uint inData[];  };
layout(std430, binding=1) buffer Out           { uint outData[]; };
layout(std430, binding=2) buffer Sums          { uint sums[];    };

uniform uint uN;

shared uint s[256];

void main() {
    uint gid   = gl_GlobalInvocationID.x;
    uint lid   = gl_LocalInvocationID.x;
    uint block = gl_WorkGroupID.x;

    uint v = (gid < uN) ? inData[gid] : 0u;
    s[lid] = v;
    barrier();

    for (uint offset = 1u; offset < 256u; offset <<= 1u) {
        uint add = (lid >= offset) ? s[lid - offset] : 0u;
        barrier();
        s[lid] += add;
        barrier();
    }

    if (gid < uN) {
        outData[gid] = s[lid] - v;
    }

    if (lid == 255u) {
        sums[block] = s[255u];
    }
}
