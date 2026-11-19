#version 450
// One eye's picture through the lens correction. Same formula as
// Vr_SampleUV (render/vr_layout.h).

layout(set = 2, binding = 0) uniform sampler2D eyeImage;

layout(std140, set = 3, binding = 0) uniform LensUniforms {
    vec4 image;     // centre x, y and half width, height of the picture, in screen pixels
    vec4 lens;      // k1, k2
};

layout(location = 0) out vec4 outColor;

float lensF(float r, float k1, float k2)
{
    float r2 = r * r;
    return 1.0 + k1 * r2 + k2 * r2 * r2;
}

void main()
{
    vec2 d = gl_FragCoord.xy - image.xy;
    float halfDiagonal = length(image.zw);
    float r = length(d) / halfDiagonal;
    float re = min(image.z, image.w) / halfDiagonal;
    float ratio = lensF(r, lens.x, lens.y) / lensF(re, lens.x, lens.y);
    vec2 uv = 0.5 + d * ratio / (2.0 * image.zw);
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    outColor = vec4(texture(eyeImage, uv).rgb, 1.0);
}
