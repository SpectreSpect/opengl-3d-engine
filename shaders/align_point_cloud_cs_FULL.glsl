#version 460
layout(local_size_x = 256) in;

struct TestOutput {
    dvec4 out_val;
};

layout(std430, binding=9) coherent buffer TestOutputTemp {TestOutput test_output;};

void main() {
    uint source_point_id = gl_GlobalInvocationID.x;
    
    if (source_point_id != 0) return;

    double some_array[6] = double[6](1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF, 1.0000009999999999LF);
    double a[6][6];

    for (int i = 0; i < 6; i++) {
        for(int j = 0; j < 6; j++) {
            a[i][j] = 0.0LF;
        }
    }

    int c = 0;
    while (true) {
        a[c][0] = some_array[0];
        break;
    }
    a[c][0] = some_array[0];
    // test_output.out_val.x = a[0][0];
    // test_output.out_val.x = some_array[c];
    a[0][0] = some_array[0];
    test_output.out_val.x = a[0][0];
}
