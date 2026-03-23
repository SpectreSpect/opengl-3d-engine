#version 460
layout(local_size_x = 256) in;

// #include "utils.glsl"

// struct PointInstance {
//     vec4 position;
//     vec4 color;
// };

// struct PointNormal {
//     vec4 normal;
// };

// struct HAndG {
//     double h[6][6];
//     double g[6];
//     uint status; // lock flag
// };

// struct Location {
//     vec4 position;
//     vec4 rotation;
// };

struct TestOutput {
    dvec4 out_val;
};


// struct OutputHG {
//     double h[6][6];
//     double g[6];
// };

// struct TracePoint {
//     uint counter;
//     uint site_id;
//     double value;
//     uint pad0;
// };



// mat3 euler_xyz_to_mat3(vec3 euler)

// #define HASH_KEY_TYPE uvec2
// #define HASH_VALUE_TYPE uint
// #define HASH_TABLE hash_table
// #define HASH_TABLE_COUNT_TOMB count_tomb
// #define HASH_TABLE_SLOT_INIT_FUNC slot_init_func
// #define KEY_HASH_FUNC hash_uvec2
// #define HASH_TABLE_SIZE u_hash_table_size

// struct HashTableSlot {
//     HASH_KEY_TYPE hash_key;
//     HASH_VALUE_TYPE hash_value;
//     uint hash_state;
// };

// layout(std430, binding=0) coherent buffer HashTable {uint count_tomb; HashTableSlot hash_table[]; }; 
// layout(std430, binding=1) coherent buffer MapPoints {PointInstance map_points[]; }; 
// layout(std430, binding=2) coherent buffer MapNormals {PointNormal map_normals[];}; 
// layout(std430, binding=3) coherent buffer NumPoints {uint num_map_points; }; 

// layout(std430, binding=4) coherent buffer SourcePoints {PointInstance source_points[];}; 
// layout(std430, binding=5) coherent buffer SourceNormals {PointNormal source_normals[];};

// layout(std430, binding=6) coherent buffer SharedHAndG {HAndG h_and_g;};

// layout(std430, binding=7) coherent buffer PointCloudLocation {Location source_location;};

layout(std430, binding=9) coherent buffer TestOutputTemp {TestOutput test_output;};

// layout(std430, binding=10) coherent buffer TargetIds {uint target_ids[];};
// layout(std430, binding=11) coherent buffer OutputHGValues {OutputHG output_hg[];};
// layout(std430, binding=12) coherent buffer TraceSSBO {TracePoint trace_points[];};
// layout(std430, binding=13) coherent buffer NumTracePointsSSBO {uint num_trace_points;};





// uniform uint num_source_points;

// HASH_VALUE_TYPE slot_init_func(out bool is_success) {
//     return 0;
// }

// uniform uint u_hash_table_size;



// #include "hash_table.glsl"


// // bool next_pos(ivec3 origin, int layer, out ivec3 index_pos, out ivec3 out_pos) {
// //     float dim_size = 1 + layer * 2;

// //     int x = index_pos.x;
// //     int y = index_pos.y;
// //     int z = index_pos.z;
    
// //     if ((z == 0 || z == (dim_size - 1)) || (y == 0 || y == (dim_size - 1))) {
// //         x++;
// //     }
// //     else {
// //         if (x == dim_size - 1)
// //             x++;
// //         else
// //             x = dim_size - 1;
// //     }

// //     if (x >= dim_size) {
// //         x = 0;
// //         if (z > dim_size)
// //             z = 0;
// //         else 
// //             z++;

// //         if (z >= dim_size) {
// //             z = 0;
// //             y++;

// //             if (y >= dim_size)
// //                 return true;
// //         }
            
// //         else 
// //             z++;
// //     }

// //     out_pos = origin + ivec3(x, y, z) - layer;

// //     return false;
// // }

// mat3 euler_xyz_to_mat3(vec3 euler)
// {
//     float cx = cos(euler.x);
//     float sx = sin(euler.x);

//     float cy = cos(euler.y);
//     float sy = sin(euler.y);

//     float cz = cos(euler.z);
//     float sz = sin(euler.z);

//     mat3 rx = mat3(
//         1.0, 0.0, 0.0,
//         0.0,  cx,  sx,
//         0.0, -sx,  cx
//     );

//     mat3 ry = mat3(
//          cy, 0.0, -sy,
//          0.0, 1.0,  0.0,
//          sy, 0.0,  cy
//     );

//     mat3 rz = mat3(
//          cz,  sz, 0.0,
//         -sz,  cz, 0.0,
//          0.0, 0.0, 1.0
//     );

//     return rx * ry * rz;
// }

// vec3 safe_normalize(vec3 v)
// {
//     float len2 = dot(v, v);
//     if (len2 < 1e-12)
//         return vec3(0.0);
//     return v * inversesqrt(len2);
// }

// vec3 transform_normal_world(vec3 cloud_rotation,
//                             vec3 cloud_scale,
//                             vec3 local_n)
// {
//     if (dot(local_n, local_n) < 1e-12)
//         return vec3(0.0);

//     mat3 R = euler_xyz_to_mat3(cloud_rotation);

//     mat3 S = mat3(1.0);
//     S[0][0] = cloud_scale.x;
//     S[1][1] = cloud_scale.y;
//     S[2][2] = cloud_scale.z;

//     mat3 linear = R * S;
//     mat3 normal_matrix = transpose(inverse(linear));

//     return safe_normalize(normal_matrix * local_n);
// }

// vec3 transform_point_world(vec3 cloud_rotation,
//                            vec3 cloud_scale,
//                            vec3 cloud_position,
//                            vec3 local_p)
// {
//     mat3 R = euler_xyz_to_mat3(cloud_rotation);
//     vec3 scaled = local_p * cloud_scale;
//     return R * scaled + cloud_position;
// }

// mat3 covariance_from_normal(vec3 raw_n, float eps)
// {
//     float len2 = dot(raw_n, raw_n);
//     if (len2 < 1e-12)
//         return mat3(0.0);

//     vec3 n = raw_n / sqrt(len2);
//     mat3 I = mat3(1.0);
//     mat3 nnT = outerProduct(n, n);

//     // I - nn^T + eps * nn^T
//     return I - (1.0 - eps) * nnT;
// }


// // uint find_closest_id(vec3 position, float max_dist_sq, float max_corr_dist)
// // {
// //     const uint pack_bits   = 21u;
// //     const uint pack_offset = 1048575u;

// //     float voxel_size = 1.0;
// //     ivec3 voxel_pos = ivec3(floor(position / voxel_size));

// //     // int num_layers = 3;
// //     int num_layers = int(ceil(max_corr_dist / voxel_size)) + 1;
    
// //     uint min_id = INVALID_ID;
// //     float min_dist = max_dist_sq;

// //     for (int layer = 0; layer < num_layers; layer++) {
// //         const int dim = 1 + layer * 2;

// //         for (int y = 0; y < dim; y++) {
// //             const bool on_y_edge = (y == 0 || y == dim - 1);

// //             for (int z = 0; z < dim; z++) {
// //                 const bool on_z_edge = (z == 0 || z == dim - 1);

// //                 int x = 0;
// //                 while (x < dim) {
// //                     ivec3 new_voxel_pos = voxel_pos + ivec3(x, y, z) - ivec3(layer);

// //                     uvec2 key = pack_key_uvec2(new_voxel_pos, pack_offset, pack_bits);

// //                     uint map_point_id = lookup_chunk(key);
                    
// //                     if (map_point_id != INVALID_ID) {
// //                         map_point_id = hash_table[map_point_id].hash_value;

// //                         vec3 to_point = map_points[map_point_id].position.xyz - position;
// //                         float dist_sq = dot(to_point, to_point);

// //                         if (dist_sq < min_dist) {
// //                             min_dist = dist_sq;
// //                             min_id = map_point_id;
// //                         }
// //                     }

// //                     if (on_y_edge || on_z_edge) {
// //                         x++;
// //                     } else {
// //                         x = (x == dim - 1) ? x + 1 : dim - 1;
// //                     }
// //                 }
// //             }
// //         }

// //         if (min_id != INVALID_ID)
// //             break;
// //     }
// //     return min_id;
// // }

// uint find_closest_id(
//     vec3 position,
//     vec4 normal4,
//     float max_dist_sq,
//     float max_corr_dist,
//     float min_normal_dot
// )
// {
//     const uint pack_bits   = 21u;
//     const uint pack_offset = 1048575u;

//     float voxel_size = 1.0;
//     ivec3 voxel_pos = ivec3(floor(position / voxel_size));

//     int num_layers = int(ceil(max_corr_dist / voxel_size)) + 1;

//     uint min_id = INVALID_ID;
//     float min_dist = max_dist_sq;

//     vec3 query_normal = normal4.xyz;
//     bool use_normal_check = dot(query_normal, query_normal) > 1e-12;

//     if (use_normal_check) {
//         query_normal = normalize(query_normal);
//     }

//     for (int layer = 0; layer < num_layers; layer++) {
//         const int dim = 1 + layer * 2;

//         for (int y = 0; y < dim; y++) {
//             const bool on_y_edge = (y == 0 || y == dim - 1);

//             for (int z = 0; z < dim; z++) {
//                 const bool on_z_edge = (z == 0 || z == dim - 1);

//                 int x = 0;
//                 while (x < dim) {
//                     ivec3 new_voxel_pos = voxel_pos + ivec3(x, y, z) - ivec3(layer);

//                     uvec2 key = pack_key_uvec2(new_voxel_pos, pack_offset, pack_bits);

//                     uint map_point_id = lookup_chunk(key);

//                     if (map_point_id != INVALID_ID) {
//                         map_point_id = hash_table[map_point_id].hash_value;

//                         vec3 candidate_pos = map_points[map_point_id].position.xyz;
//                         vec3 to_point = candidate_pos - position;
//                         float dist_sq = dot(to_point, to_point);

//                         if (dist_sq < min_dist) {
//                             bool normal_ok = true;

//                             if (use_normal_check) {
//                                 vec3 candidate_normal = map_normals[map_point_id].normal.xyz;
//                                 float nlen_sq = dot(candidate_normal, candidate_normal);

//                                 if (nlen_sq < 1e-12) {
//                                     normal_ok = false;
//                                 } else {
//                                     candidate_normal *= inversesqrt(nlen_sq);

//                                     float ndot = dot(query_normal, candidate_normal);

//                                     // If normal orientation may be flipped, use this instead:
//                                     // ndot = abs(ndot);

//                                     if (ndot < min_normal_dot) {
//                                         normal_ok = false;
//                                     }
//                                 }
//                             }

//                             if (normal_ok) {
//                                 min_dist = dist_sq;
//                                 min_id = map_point_id;
//                             }
//                         }
//                     }

//                     if (on_y_edge || on_z_edge) {
//                         x++;
//                     } else {
//                         x = (x == dim - 1) ? x + 1 : dim - 1;
//                     }
//                 }
//             }
//         }

//         // if (min_id != INVALID_ID)
//         //     break;
//     }

//     return min_id;
// }

// uint find_closest_id_brute_force(
//     vec3 position,
//     vec4 normal4,
//     float max_dist_sq,
//     float max_corr_dist,
//     float min_normal_dot
// ) {
//     if (num_map_points == 0u)
//         return 0xFFFFFFFFu; // equivalent of -1

//     uint min_id = 0xFFFFFFFFu;
//     float min_dist = max_dist_sq;

//     for (uint i = 0u; i < num_map_points; i++) {
//         vec3 target_normal = map_normals[i].normal.xyz;

//         // skip invalid normals
//         if (dot(target_normal, target_normal) < 1e-12)
//             continue;

//         vec3 d = map_points[i].position.xyz - position;
//         float dist = dot(d, d); // squared distance

//         if (dist < min_dist) {
//             min_dist = dist;
//             min_id = i;
//         }
//     }

//     return min_id;
// }

// mat3 skew_matrix(vec3 v)
// {
//     return mat3(
//          0.0,  v.z, -v.y,
//         -v.z,  0.0,  v.x,
//          v.y, -v.x,  0.0
//     );
// }

// void add_mat3_block(inout double H[6][6], int row0, int col0, mat3 M)
// {
//     for (int r = 0; r < 3; r++) {
//         for (int c = 0; c < 3; c++) {
//             H[row0 + r][col0 + c] += double(M[c][r]); // mat3 is also [col][row]
//         }
//     }
// }

// void add_vec3_block(inout double g[6], int row0, vec3 v)
// {
//     for (int r = 0; r < 3; r++) {
//         g[row0 + r] += double(v[r]);
//     }
// }

// // bool solve_6x6(in double H_in[6][6], in double g_in[6], out double delta_out[6])
// // {
// //     double delta_local[6];

// //     // for (int i = 0; i < 6; i++) {
// //     //     delta_local[i] = H_in[0][0];
// //     // }

// //     const double EPS = double(1.0e-12);

// //     // Augmented matrix [H | g], size 6 x 7
// //     double a[6][7];

// //     for (int r = 0; r < 6; r++) {
// //         for (int c = 0; c < 6; c++) {
// //             a[r][c] = H_in[r][c];
// //         }
// //         a[r][6] = g_in[r];
// //     }

// //     // Forward elimination with partial pivoting
// //     for (int col = 0; col < 6; col++) {
// //         // Find pivot row
// //         int pivot_row = col;
// //         double max_abs = abs(a[col][col]);

// //         for (int r = col + 1; r < 6; r++) {
// //             double v = abs(a[r][col]);
// //             if (v > max_abs) {
// //                 max_abs = v;
// //                 pivot_row = r;
// //             }
// //         }

// //         // Singular / degenerate check
// //         if (max_abs < EPS) {
// //             return false;
// //         }

// //         // Swap rows if needed
// //         if (pivot_row != col) {
// //             for (int c = col; c < 7; c++) {
// //                 double tmp = a[col][c];
// //                 a[col][c] = a[pivot_row][c];
// //                 a[pivot_row][c] = tmp;
// //             }
// //         }

// //         // Eliminate rows below
// //         for (int r = col + 1; r < 6; r++) {
// //             double factor = a[r][col] / a[col][col];

// //             for (int c = col; c < 7; c++) {
// //                 a[r][c] -= factor * a[col][c];
// //             }
// //         }
// //     }

// //     // Back substitution
// //     for (int r = 5; r >= 0; r--) {
// //         double sum = a[r][6];

// //         for (int c = r + 1; c < 6; c++) {
// //             sum -= a[r][c] * delta_local[c];
// //         }

// //         if (abs(a[r][r]) < EPS) {
// //             return false;
// //         }

// //         delta_local[r] = sum / a[r][r];
// //         // delta_local[r] += 5;
// //     }

// //     for (int i = 0; i < 6; i++) {
// //         delta_out[i] = delta_local[i];
// //     }

// //     return true;
// // }

// // void trace_dump(uint trace_counter, uint site_id, double value)
// // {
// //     trace_points[trace_counter].counter = trace_counter;
// //     trace_points[trace_counter].site_id = site_id;
// //     trace_points[trace_counter].value   = value;
// //     trace_points[trace_counter].pad0    = 0u;
// //     trace_counter++;
// // }

// void trace_dump(double value)
// {
//     // trace_points[0u].value = value;
//     test_output.out_val.y = value;
// }


// bool solve_6x6(in double H_in[6][6], in  double g_in[6], out double delta_out[6])
// {
//     // double a[6][7];
//     // int c = 1;
//     // while (true) {
//     //     a[c][1] = H_in[1][1];
//     //     break;
//     // }
//     // test_output.out_val.x = a[1][1];

//     // for (int col = 0; col < 2; col++) {
//     //     for (int r = 0; r < 6; r++) {
//     //         trace_dump(a[r][col]);
//     //     }
//     // }

//     // uint trace_counter = 0u;

//     // precise double delta_local[6];

//     // precise const double EPS = double(1.0e-12);

//     // Augmented matrix [H | g], size 6 x 7
//     // precise double a[6][7];

//     // int col = 1;
//     // trace_dump(trace_counter, 7u, a[col][col]);
//     // trace_counter++;

//     // int col = 1;
//     // while (col <= 2) {
//     //     trace_dump(trace_counter, 7u, a[col][col]);
//     //     col++;
//     // }

//     // for (int col = 1; col <= 1; col++) {
//     //     trace_dump(trace_counter, 7u, a[col][col]);
//     //     trace_counter++;

//     //     // trace_points[trace_counter].counter = trace_counter;
//     //     // trace_points[trace_counter].site_id = 7u;
//     //     // trace_points[trace_counter].value   = a[col][col];
//     //     // trace_points[trace_counter].pad0    = 0u;
//     //     // trace_counter++;
//     // }




    
//     // a[1][1] = H_in[1][1];
    

//     // for (uint r = 0u; r < 6u; r++) {
//     //     // trace_dump(trace_counter, 1u, double(r));

//     //     for (uint c = 0u; c < 6u; c++) {
//     //         // trace_dump(trace_counter, 2u, double(c));

//     //         a[r][c] = H_in[r][c];
//     //         // trace_dump(trace_counter, 3u, a[r][c]);
//     //     }

//     //     // a[r][6] = g_in[r];
//     //     // trace_dump(trace_counter, 4u, a[r][6]);
//     // }

//     // a[1][1] = H_in[1][1];/
//     // test_output.out_val.x = a[1][1];


    
    

//     // Forward elimination with partial pivoting
//     // for (int col = 0; col < 6; col++) {
//     //     // trace_dump(trace_counter, 5u, double(col));

//     //     // Find pivot row
//     //     // int pivot_row = col;
//     //     // trace_dump(trace_counter, 6u, double(pivot_row));

//     //     // double max_abs = abs(a[col][col]);

//     //     for (int r = col + 1; r < 6; r++) {
//     //         // trace_dump(trace_counter, 8u, double(r));

//     //         // double v = abs(a[r][col]);
//     //         trace_dump(trace_counter, 9u, a[r][col]);

//     //         // if (v > max_abs) {
//     //         //     trace_dump(trace_counter, 10u, 1.0);

//     //         //     max_abs = v;
//     //         //     trace_dump(trace_counter, 11u, max_abs);

//     //         //     pivot_row = r;
//     //         //     trace_dump(trace_counter, 12u, double(pivot_row));
//     //         // }
//     //     }

//     //     // // Singular / degenerate check
//     //     // if (max_abs < EPS) {
//     //     //     trace_dump(trace_counter, 13u, 1.0);
//     //     //     return false;
//     //     // }

//     //     // // Swap rows if needed
//     //     // if (pivot_row != col) {
//     //     //     trace_dump(trace_counter, 14u, 1.0);

//     //     //     for (int c = col; c < 7; c++) {
//     //     //         trace_dump(trace_counter, 15u, double(c));

//     //     //         double tmp = a[col][c];
//     //     //         a[col][c] = a[pivot_row][c];
//     //     //         a[pivot_row][c] = tmp;

//     //     //         trace_dump(trace_counter, 16u, a[col][c]);
//     //     //         trace_dump(trace_counter, 17u, a[pivot_row][c]);
//     //     //     }
//     //     // }

//     //     // // Eliminate rows below
//     //     // for (int r = col + 1; r < 6; r++) {
//     //     //     trace_dump(trace_counter, 18u, double(r));

//     //     //     double factor = a[r][col] / a[col][col];
//     //     //     trace_dump(trace_counter, 19u, factor);

//     //     //     for (int c = col; c < 7; c++) {
//     //     //         trace_dump(trace_counter, 20u, double(c));

//     //     //         a[r][c] -= factor * a[col][c];
//     //     //         trace_dump(trace_counter, 21u, a[r][c]);
//     //     //     }
//     //     // }
//     // }

//     // // Back substitution
//     // for (int r = 5; r >= 0; r--) {
//     //     trace_dump(trace_counter, 22u, precise double(r));

//     //     precise double sum = a[r][6];
//     //     trace_dump(trace_counter, 23u, sum);

//     //     for (int c = r + 1; c < 6; c++) {
//     //         trace_dump(trace_counter, 24u, precise double(c));

//     //         sum -= a[r][c] * delta_local[c];
//     //         trace_dump(trace_counter, 25u, sum);
//     //     }

//     //     if (abs(a[r][r]) < EPS) {
//     //         trace_dump(trace_counter, 26u, 1.0);
//     //         return false;
//     //     }

//     //     delta_local[r] = sum / a[r][r];
//     //     trace_dump(trace_counter, 27u, delta_local[r]);
//     // }

//     // // Not traced on purpose, so trace site numbering stays identical to the C++ version
//     // for (int i = 0; i < 6; i++) {
//     //     delta_out[i] = delta_local[i];
//     // }

//     // trace_dump(trace_counter, 28u, 1.0);
//     // num_trace_points = trace_counter + 1;
//     return true;
// }

// bool test_func_sdf(in precise double H_in[6][6], in precise double g_in[6], out precise double delta_out[6]) {
//     // precise double a[6][7];
//     // int c = 1;
//     // while (true) {
//     //     a[c][1] = H_in[1][1];
//     //     // r += 1;
//     //     break;
//     // }
//     // test_output.out_val.x = a[1][1];


//     // uint trace_counter = 0u;

//     // precise double delta_local[6];

//     // precise const double EPS = double(1.0e-12);

//     // Augmented matrix [H | g], size 6 x 7
//     // precise double a[6][7];

    
//     // // a[1][1] = H_in[1][1];
    

//     // for (uint r = 0u; r < 6u; r++) {
//     //     // trace_dump(trace_counter, 1u, double(r));

//     //     for (uint c = 0u; c < 6u; c++) {
//     //         // trace_dump(trace_counter, 2u, double(c));

//     //         a[r][c] = H_in[r][c];
//     //         // trace_dump(trace_counter, 3u, a[r][c]);
//     //     }

//     //     // a[r][6] = g_in[r];
//     //     // trace_dump(trace_counter, 4u, a[r][6]);
//     // }

//     // // a[1][1] = H_in[1][1];/
//     // test_output.out_val.x = a[1][1];
    
    

//     // // Forward elimination with partial pivoting
//     // for (int col = 0; col < 6; col++) {
//     //     trace_dump(trace_counter, 5u, double(col));

//     //     // Find pivot row
//     //     int pivot_row = col;
//     //     trace_dump(trace_counter, 6u, double(pivot_row));

//     //     precise double max_abs = abs(a[col][col]);
//     //     trace_dump(trace_counter, 7u, max_abs);

//     //     for (int r = col + 1; r < 6; r++) {
//     //         trace_dump(trace_counter, 8u, double(r));

//     //         precise double v = abs(a[r][col]);
//     //         trace_dump(trace_counter, 9u, v);

//     //         if (v > max_abs) {
//     //             trace_dump(trace_counter, 10u, 1.0);

//     //             max_abs = v;
//     //             trace_dump(trace_counter, 11u, max_abs);

//     //             pivot_row = r;
//     //             trace_dump(trace_counter, 12u, precise double(pivot_row));
//     //         }
//     //     }

//     //     // Singular / degenerate check
//     //     if (max_abs < EPS) {
//     //         trace_dump(trace_counter, 13u, 1.0);
//     //         return false;
//     //     }

//     //     // Swap rows if needed
//     //     if (pivot_row != col) {
//     //         trace_dump(trace_counter, 14u, 1.0);

//     //         for (int c = col; c < 7; c++) {
//     //             trace_dump(trace_counter, 15u, precise double(c));

//     //             precise double tmp = a[col][c];
//     //             a[col][c] = a[pivot_row][c];
//     //             a[pivot_row][c] = tmp;

//     //             trace_dump(trace_counter, 16u, a[col][c]);
//     //             trace_dump(trace_counter, 17u, a[pivot_row][c]);
//     //         }
//     //     }

//     //     // Eliminate rows below
//     //     for (int r = col + 1; r < 6; r++) {
//     //         trace_dump(trace_counter, 18u, precise double(r));

//     //         precise double factor = a[r][col] / a[col][col];
//     //         trace_dump(trace_counter, 19u, factor);

//     //         for (int c = col; c < 7; c++) {
//     //             trace_dump(trace_counter, 20u, precise double(c));

//     //             a[r][c] -= factor * a[col][c];
//     //             trace_dump(trace_counter, 21u, a[r][c]);
//     //         }
//     //     }
//     // }

//     // // Back substitution
//     // for (int r = 5; r >= 0; r--) {
//     //     trace_dump(trace_counter, 22u, precise double(r));

//     //     precise double sum = a[r][6];
//     //     trace_dump(trace_counter, 23u, sum);

//     //     for (int c = r + 1; c < 6; c++) {
//     //         trace_dump(trace_counter, 24u, precise double(c));

//     //         sum -= a[r][c] * delta_local[c];
//     //         trace_dump(trace_counter, 25u, sum);
//     //     }

//     //     if (abs(a[r][r]) < EPS) {
//     //         trace_dump(trace_counter, 26u, 1.0);
//     //         return false;
//     //     }

//     //     delta_local[r] = sum / a[r][r];
//     //     trace_dump(trace_counter, 27u, delta_local[r]);
//     // }

//     // // Not traced on purpose, so trace site numbering stays identical to the C++ version
//     // for (int i = 0; i < 6; i++) {
//     //     delta_out[i] = delta_local[i];
//     // }

//     // trace_dump(trace_counter, 28u, 1.0);
//     // num_trace_points = trace_counter + 1;

//     return true;
// }

// mat3 omega_to_mat3(vec3 omega)
// {
//     float theta = length(omega);

//     if (theta < 1e-12)
//         return mat3(1.0);

//     vec3 axis = omega / theta;
//     mat3 K = skew_matrix(axis);

//     // Rodrigues rotation formula
//     return mat3(1.0) + sin(theta) * K + (1.0 - cos(theta)) * (K * K);
// }

// vec3 mat3_to_euler_xyz(mat3 R)
// {
//     const float EPS = 1e-6;
//     const float HALF_PI = 1.57079632679;

//     float x, y, z;

//     // GLM-compatible convention here:
//     // R = Rx(x) * Ry(y) * Rz(z)

//     // row 0, col 2  -> GLSL: R[2][0]
//     float sy = clamp(R[2][0], -1.0, 1.0);

//     if (sy >= 1.0 - EPS) {
//         y = HALF_PI;
//         z = 0.0;
//         x = atan(R[0][1], R[1][1]);
//     }
//     else if (sy <= -1.0 + EPS) {
//         y = -HALF_PI;
//         z = 0.0;
//         x = atan(-R[0][1], R[1][1]);
//     }
//     else {
//         y = asin(sy);

//         // row 1, col 2 = R[2][1]
//         // row 2, col 2 = R[2][2]
//         x = atan(-R[2][1], R[2][2]);

//         // row 0, col 1 = R[1][0]
//         // row 0, col 0 = R[0][0]
//         z = atan(-R[1][0], R[0][0]);
//     }

//     return vec3(x, y, z);
// }

// double step()
// {
//     // const std::vector<PointInstance>& source_points = source_point_cloud.points;
//     // const std::vector<PointInstance>& target_points = target_point_cloud.points;

//     // if (source_points.size() != source_normals.size()) {
//     //     std::cout << "source_points.size() != source_normals.size()\n";
//     //     return -1.0;
//     // }

//     // if (target_points.size() != target_normals.size()) {
//     //     std::cout << "target_points.size() != target_normals.size()\n";
//     //     return -1.0;
//     // }

//     float max_corr_dist = 10.0;
//     float max_corr_dist_sq = max_corr_dist * max_corr_dist;
//     float max_rot = radians(5.0);
//     float max_trans = 5.0;
//     float gicp_eps = 1e-3;
//     // float min_normal_dot = cos(radians(30.0)); 
//     float min_normal_dot = -1.0;

//     // double H[6][6] = {};
//     // double g[6] = {};

//     double H[6][6];
//     double g[6];

//     for (int i = 0; i < 6; ++i) {
//         g[i] = 0.0;
//         for (int j = 0; j < 6; ++j) {
//             H[i][j] = 0.0;
//         }
//     }

//     // Precompute target points, normals, covariances in world space
//     // std::vector<glm::vec3> target_points_world;
//     // std::vector<glm::vec3> target_normals_world;
//     // std::vector<glm::mat3> target_covs_world;

//     // target_points_world.reserve(target_points.size());
//     // target_normals_world.reserve(target_points.size());
//     // target_covs_world.reserve(target_points.size());

//     // std::vector<float> source_distances(source_points.size(), -1.0f);
//     // std::vector<float> target_indices(source_points.size(), -1.0f);
//     float max_new_dist = 0.0f;

//     // for (size_t i = 0; i < target_points.size(); i++) {
//     //     glm::vec3 q_local = glm::vec3(target_points[i].pos);

//     //     // Convert vec4 normal -> vec3 normal (ignore w)
//     //     glm::vec3 n_local = glm::vec3(target_normals[i]);
//     //     glm::vec3 n_world = transform_normal_world(target_point_cloud, n_local);

//     //     target_points_world.push_back(transform_point_world(target_point_cloud, q_local));
//     //     target_normals_world.push_back(n_world);
//     //     target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));
//     // }

//     double total_weighted_sq_error = 0.0;
//     int valid_count = 0;

//     for (uint i = 0; i < num_source_points; i++) {
        
//         vec3 p_local = vec3(source_points[i].position);

//         // Convert vec4 normal -> vec3 normal (ignore w)
//         vec3 n_src_local = vec3(source_normals[i].normal);


//         // vec3 transform_point_world(vec3 cloud_rotation,
//         //                    vec3 cloud_scale,
//         //                    vec3 cloud_position,
//         //                    vec3 local_p)

//         // Source point and source normal in world space

//         vec3 source_rotation = source_location.rotation.xyz;
//         vec3 source_scale = vec3(1.0, 1.0, 1.0);
//         vec3 source_position = source_location.position.xyz;

//         vec3 x = transform_point_world(source_rotation, source_scale, source_position, p_local);

//         // vec3 transform_normal_world(vec3 cloud_rotation,
//         //                     vec3 cloud_scale,
//         //                     vec3 local_n)

//         vec3 n_src_world = transform_normal_world(source_rotation, source_scale, n_src_local);

        
        

//         if (dot(n_src_world, n_src_world) < 1e-12) {
//             continue;
//         }

        

        

//         // uint find_closest_id(vec3 position, float max_dist_sq)

//         // int target_id = find_closest_id_with_valid_normal(
//         //     target_points_world,
//         //     target_normals_world,
//         //     x,
//         //     max_corr_dist_sq
//         // );

//         // uint find_closest_id(vec3 position, float max_dist_sq)

//         // uint target_id = find_closest_id(x, vec4(n_src_world, 1.0), max_corr_dist_sq, max_corr_dist, min_normal_dot);
//         uint target_id = find_closest_id_brute_force(x, vec4(n_src_world, 1.0), max_corr_dist_sq, max_corr_dist, min_normal_dot);


            

        

//         // source_location.position.x = i;
        
//         if (target_id == INVALID_ID) {
//             continue;
//         }

//         vec3 target_position = map_points[target_id].position.xyz;

//         // if (i == 0) {
//         //     source_location.position.xyz = target_position;
//         // }   

        

//         // source_distances[i]? = distance(target_position, x);

//         // if (source_distances[i] > max_new_dist) {
//         //     max_new_dist = source_distances[i];
//         // }

//         // target_indices[i] = static_cast<float>(target_id);

//         // source_point_cloud.points[i].color = glm::vec4(1, 0, 0, 1);

//         vec3 q = target_position;
//         vec3 n_tgt_world = map_normals[target_id].normal.xyz;

//         // if (dot(n_tgt_world, n_tgt_world) < 1e-12) {
//         //     continue;
//         // }

        

//         // Build source and target local covariances
//         // mat3 covariance_from_normal(vec3 raw_n, float eps)

//         mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
        
//         // target_covs_world.push_back(covariance_from_normal(n_world, gicp_eps));

//         mat3 C_B = covariance_from_normal(n_tgt_world, gicp_eps);

//         // GICP weighting matrix:
//         // M = (C_B + C_A)^-1
//         mat3 Sigma = C_B + C_A;
//         mat3 M = inverse(Sigma);

//         vec3 d = q - x;

//         // Linearization:
//         // x' ≈ x + ω × x + v
//         // d' = q - x' ≈ d + x×ω - v
//         // so A = [skew(x), -I], residual = d

//         // mat3 skew_matrix(vec3 v)
//         mat3 B = skew_matrix(x);

//         mat3 H00 = transpose(B) * M * B;
//         mat3 H01 = -transpose(B) * M;
//         mat3 H10 = -M * B;
//         mat3 H11 = M;

//         vec3 g0 = -transpose(B) * M * d;
//         vec3 g1 = M * d;

//         // void add_mat3_block(inout double H[6][6], int row0, int col0, mat3 M)

//         add_mat3_block(H, 0, 0, H00);
//         add_mat3_block(H, 0, 3, H01);
//         add_mat3_block(H, 3, 0, H10);
//         add_mat3_block(H, 3, 3, H11);

//         // void add_vec3_block(inout double g[6], int row0, vec3 v)

//         add_vec3_block(g, 0, g0);
//         add_vec3_block(g, 3, g1);

//         total_weighted_sq_error += dot(d, M * d);
//         valid_count++;
//     }
    
//     if (valid_count < 6) {
//         // std::cout << "Too few correspondences\n";
//         return -1.0;
//     }

//     double rmse = sqrt(total_weighted_sq_error / double(valid_count));

//     double lambda = 1e-6;
//     for (int i = 0; i < 6; i++) {
//         H[i][i] += lambda;
//     }

//     double delta[6];

//     for (int i = 0; i < 6; i++)
//         delta[i] = 0.0;

//     bool ok = solve_6x6(H, g, delta);
//     if (!ok) {
//         // std::cout << "solve_6x6 failed\n";
//         return -1.0;
//     }

//     vec3 omega = vec3(float(delta[0]), float(delta[1]), float(delta[2]));
//     vec3 v = vec3(float(delta[3]), float(delta[4]), float(delta[5]));

//     float omega_len = length(omega);
//     if (omega_len > max_rot) {
//         omega *= max_rot / omega_len;
//     }

//     float v_len = length(v);
//     if (v_len > max_trans) {
//         v *= max_trans / v_len;
//     }

//     // mat3 omega_to_mat3(vec3 omega)

//     mat3 dR = omega_to_mat3(omega);

//     // mat3 euler_xyz_to_mat3(vec3 euler)

//     // source_location.rotation.xyz
//     // source_location.position.xyz

//     mat3 R_src = euler_xyz_to_mat3(source_location.rotation.xyz);
//     mat3 R_src_new = dR * R_src;
//     vec3 t_src_new = dR * source_location.position.xyz + v;

//     // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//     source_location.position.xyz = t_src_new;
//     source_location.rotation.xyz = mat3_to_euler_xyz(R_src_new);
//     // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


//     // std::cout << "valid_count = " << valid_count
//     //           << ", weighted_rmse = " << rmse
//     //           << ", |omega| = " << glm::length(omega)
//     //           << ", |v| = " << glm::length(v)
//     //           << "\n";

//     return rmse;
// }


// double step_test()
// {
//     float max_corr_dist    = 10.0;
//     float max_corr_dist_sq = max_corr_dist * max_corr_dist;
//     float max_rot          = radians(5.0);
//     float max_trans        = 5.0;
//     float gicp_eps         = 1e-3;
//     float min_normal_dot   = -1.0;

//     double H[6][6];
//     double g[6];

//     for (int i = 0; i < 6; ++i) {
//         g[i] = 0.0;
//         for (int j = 0; j < 6; ++j) {
//             H[i][j] = 0.0;
//         }
//     }

//     double total_weighted_sq_error = 0.0;
//     int valid_count = 0;

//     vec3 source_rotation = source_location.rotation.xyz;
//     vec3 source_scale    = vec3(1.0, 1.0, 1.0);
//     vec3 source_position = source_location.position.xyz;

//     for (uint i = 0u; i < num_source_points; ++i) {
//         vec3 p_local = vec3(source_points[i].position);
//         vec3 n_src_local = vec3(source_normals[i].normal);

//         vec3 x = transform_point_world(
//             source_rotation,
//             source_scale,
//             source_position,
//             p_local
//         );

//         vec3 n_src_world = transform_normal_world(
//             source_rotation,
//             source_scale,
//             n_src_local
//         );

//         if (dot(n_src_world, n_src_world) < 1e-12) {
//             continue;
//         }

//         uint target_id = INVALID_ID;
//         float best_dist_sq = max_corr_dist_sq;

//         for (uint j = 0u; j < num_map_points; ++j) {
//             vec3 n_tgt_world = map_normals[j].normal.xyz;

//             if (dot(n_tgt_world, n_tgt_world) < 1e-12) {
//                 continue;
//             }

//             vec3 q = map_points[j].position.xyz;
//             vec3 diff = q - x;
//             float dist_sq = dot(diff, diff);

//             if (dist_sq >= best_dist_sq) {
//                 continue;
//             }

//             if (min_normal_dot > -1.0) {
//                 float src_len = length(n_src_world);
//                 float tgt_len = length(n_tgt_world);

//                 if (src_len < 1e-12 || tgt_len < 1e-12) {
//                     continue;
//                 }

//                 float ndot = dot(n_src_world / src_len, n_tgt_world / tgt_len);
//                 if (ndot < min_normal_dot) {
//                     continue;
//                 }
//             }

//             best_dist_sq = dist_sq;
//             target_id = j;
//         }

//         if (target_id == INVALID_ID) {
//             continue;
//         }

//         target_ids[i] = target_id;


//         vec3 q = map_points[target_id].position.xyz;
//         vec3 n_tgt_world = map_normals[target_id].normal.xyz;

//         mat3 C_A = covariance_from_normal(n_src_world, gicp_eps);
//         mat3 C_B = covariance_from_normal(n_tgt_world, gicp_eps);

//         mat3 Sigma = C_B + C_A;
//         mat3 M = inverse(Sigma);

//         vec3 d = q - x;

//         mat3 B = skew_matrix(x);

//         mat3 H00 = transpose(B) * M * B;
//         mat3 H01 = -transpose(B) * M;
//         mat3 H10 = -M * B;
//         mat3 H11 = M;

//         vec3 g0 = -transpose(B) * M * d;
//         vec3 g1 = M * d;

//         add_mat3_block(H, 0, 0, H00);
//         add_mat3_block(H, 0, 3, H01);
//         add_mat3_block(H, 3, 0, H10);
//         add_mat3_block(H, 3, 3, H11);

//         add_vec3_block(g, 0, g0);
//         add_vec3_block(g, 3, g1);

//         total_weighted_sq_error += dot(d, M * d);

//         // if (i == 0)
//         //     test_output.out_val.x = float(total_weighted_sq_error);

        

//         valid_count++;
//     }

//     // if (valid_count < 6) {
//     //     return -1.0;
//     // }

//     double rmse = sqrt(total_weighted_sq_error / double(valid_count));

//     // test_output.out_val.x = float(rmse);

//     double lambda = 1e-6;
//     for (int i = 0; i < 6; ++i) {
//         H[i][i] += lambda;
//     }

//     double delta[6];
//     for (int i = 0; i < 6; ++i) {
//         delta[i] = 0.0;
//     }

//     for (int a = 0; a < 6; a++) {
//         output_hg[0].g[a] = g[a];
//         for (int b = 0; b < 6; b++) {
//             output_hg[0].h[a][b] = H[a][b];
//         }
//     }

//     bool ok = solve_6x6(H, g, delta);

//     // test_output.out_val.x = float(delta[3]);
//     // test_output.out_val.y = float(delta[4]);
//     // test_output.out_val.z = float(delta[5]);
    

//     if (!ok) {
//         return -1.0;
//     }

//     vec3 omega = vec3(
//         float(delta[0]),
//         float(delta[1]),
//         float(delta[2])
//     );

//     vec3 v = vec3(
//         float(delta[3]),
//         float(delta[4]),
//         float(delta[5])
//     );

    

//     float omega_len = length(omega);
//     if (omega_len > max_rot) {
//         omega *= max_rot / omega_len;
//     }

//     float v_len = length(v);
//     if (v_len > max_trans) {
//         v *= max_trans / v_len;
//     }

//     mat3 dR = omega_to_mat3(omega);

//     mat3 R_src = euler_xyz_to_mat3(source_location.rotation.xyz);
//     mat3 R_src_new = dR * R_src;
//     vec3 t_src_new = dR * source_location.position.xyz + v;

    

//     source_location.position.xyz = t_src_new;
//     source_location.rotation.xyz = mat3_to_euler_xyz(R_src_new);

//     return rmse;
// }



// #define STATUS_UNLOCKED 0u
// #define STATUS_LOCKED 1u

// uniform mat3 uR;





// vec3 transform_normal_world(vec3 cloud_rotation,
//                             vec3 cloud_scale,
//                             vec3 local_n)


void main() {
    uint source_point_id = gl_GlobalInvocationID.x;
    
    if (source_point_id != 0) return;

    double some_array[6] = double[6](1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF);
    double a[6][6];
    int c = 0;
    while (true) {
        // a[c][1] = test_H[1][1];
        a[c][0] = some_array[0];
        break;
    }
    test_output.out_val.x = a[0][0];

    for (int col = 0; col < 2; col++) {
        for (int r = 0; r < 6; r++) {
            double some_value = a[r][col];
            test_output.out_val.y = some_value;
        }
    }



    // double test_H[6][6] = double[6][6](
    //     double[6](5.00506440208984372e+02LF, -5.00000000000000000e-01LF, -5.00006439208984375e+02LF, 0.0LF, -5.00006439208984375e+02LF, 5.00000000000000000e-01LF),
    //     double[6](-5.00000000000000000e-01LF, 1.00000099999999992e+00LF, -5.00000000000000000e-01LF, 5.00000000000000000e-01LF, 0.0LF, -5.00000000000000000e-01LF),
    //     double[6](-5.00006439208984375e+02LF, -5.00000000000000000e-01LF, 5.00506440208984372e+02LF, -5.00000000000000000e-01LF, 5.00006439208984375e+02LF, 0.0LF),
    //     double[6](0.0LF, 5.00000000000000000e-01LF, -5.00000000000000000e-01LF, 5.00001000000000029e-01LF, 0.0LF, 0.0LF),
    //     double[6](-5.00006439208984375e+02LF, 0.0LF, 5.00006439208984375e+02LF, 0.0LF, 5.00006440208984372e+02LF, 0.0LF),
    //     double[6](5.00000000000000000e-01LF, -5.00000000000000000e-01LF, 0.0LF, 0.0LF, 0.0LF, 5.00001000000000029e-01LF)
    // );

    // double test_g[6] = double[6](
    //     4.99506439208984375e+02LF,
    //     0.0LF,
    // -4.99506439208984375e+02LF,
    // -5.00000000000000000e-01LF,
    // -5.00006439208984375e+02LF,
    // -5.00000000000000000e-01LF
    // );

    // double test_H[6][6] = double[6][6](
    //     double[6](500.50644020898437LF, -0.5LF, -500.00643920898438LF, 0LF, -500.00643920898438LF, 0.5LF),
    //     double[6](-0.5LF, 1.0000009999999999LF, -0.5LF, 0.5LF, 0LF, -0.5LF),
    //     double[6](-500.00643920898438LF, -0.5LF, 500.50644020898437LF, -0.5LF, 500.00643920898438LF, 0LF),
    //     double[6](0LF, 0.5LF, -0.5LF, 0.50000100000000003LF, 0LF, 0LF),
    //     double[6](-500.00643920898438LF, 0LF, 500.00643920898438LF, 0LF, 500.00644020898437LF, 0LF),
    //     double[6](0.5LF, -0.5LF, 0LF, 0LF, 0LF, 0.50000100000000003LF)
    // );

    // double test_g[6] = double[6](499.50643920898438LF, 0LF, -499.50643920898438LF, -0.5LF, -500.00643920898438LF, -0.5LF);
    
    // double test_delta_out[6] = {0LF, 0LF, 0LF, 0LF, 0LF, 0LF};

    
    

    // bool ok = solve_6x6(test_H, test_g, test_delta_out);


    // test_func_sdf(test_H, test_g, test_delta_out);

    // double a[6][7];


    


    // ----------------C++ output----------------
    // bool ok = 1
    // double delta[6] = {4.9227622343639727e-07, -3.2940920091484633e-09, -5.0213826444111244e-07, -0.99999849884717473, -0.99999900358553995, -0.99999849557332432};
    // ------------------------------------------

    // for (int i = 0; i < 6; i++)
    //     for (int j = 0; j < 6; j++) {
    //         output_hg[0].h[i][j] = test_H[i][j];
    //     }
    
    // for (int i = 0; i < 6; i++) {
    //     output_hg[0].g[i] = test_delta_out[i];
    //     // output_hg[0].g[i] = i;
    // }

    // if (ok)
    //     test_output.out_val.x = 1;
    // else
    //     test_output.out_val.x = 0;


    // test_output.out_val.x = 1.0000009999999999LF;
    // test_output.out_val.x = test_H[1][1];
}
