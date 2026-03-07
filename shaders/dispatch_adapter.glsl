#version 430
layout(local_size_x = 1) in;

layout(std430, binding=0) buffer Buffer0  { uint data_0[]; };
layout(std430, binding=1) buffer Buffer1  { uint data_1[]; };
layout(std430, binding=2) buffer Buffer2  { uint data_2[]; };

layout(std430, binding=3) buffer DispatchBuf { uvec3 dispatch_buf; };

uniform uint u_offset_0;
uniform uint u_offset_1;
uniform uint u_offset_2;

uniform uint u_direct_value_0;
uniform uint u_direct_value_1;
uniform uint u_direct_value_2;

uniform uint u_x_workgroup_size;
uniform uint u_y_workgroup_size;
uniform uint u_z_workgroup_size;

#define USE_UNIFORM 0xFFFFFFFFu

uint div_up_u32(uint a, uint b) { 
    return (a + b - 1u) / b; 
}

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;

    uint x = u_offset_0 != USE_UNIFORM ? data_0[u_offset_0] : u_direct_value_0;
    uint y = u_offset_1 != USE_UNIFORM ? data_1[u_offset_1] : u_direct_value_1;
    uint z = u_offset_2 != USE_UNIFORM ? data_2[u_offset_2] : u_direct_value_2;

    uint x_groups = div_up_u32(x, u_x_workgroup_size);
    uint y_groups = div_up_u32(y, u_y_workgroup_size);
    uint z_groups = div_up_u32(z, u_z_workgroup_size);

    dispatch_buf = uvec3(x_groups, y_groups, z_groups);
}