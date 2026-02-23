#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;

void main()
{
    const int radius = 25; // number of samples on one side (total taps = 2*radius+1)
    const float sigma = 10.0; // standard deviation of Gaussian

    vec2 tex_offset = 1.0 / textureSize(image, 0); // size of 1 texel
    vec3 result = vec3(0.0);
    float weight_sum = 0.0;

    for(int i = -radius; i <= radius; ++i)
    {
        // Gaussian weight
        float w = exp(-(float(i*i)) / (2.0 * sigma * sigma));

        // offset vector
        vec2 offset = horizontal ? vec2(tex_offset.x * float(i), 0.0)
                                 : vec2(0.0, tex_offset.y * float(i));

        // accumulate weighted color
        result += texture(image, TexCoords + offset).rgb * w;
        weight_sum += w;
    }

    // normalize so total weights sum to 1
    result /= weight_sum;

    FragColor = vec4(result, 1.0);
}