#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uResolution;
uniform float uCoefficient;
uniform float uRadius;

void main()
{
    vec2 texel = 1.0 / uResolution;
    vec3 base = texture(uTexture, vUV).rgb;

    float luma = dot(base, vec3(0.2126, 0.7152, 0.0722));
    vec3 thresholded = luma > 0.6 ? base : vec3(0.0);

    const float SPREAD = 12.0;
    int RADIUS = 8;
    if (uRadius > 0.0) {
        RADIUS = int(clamp(uRadius, 1.0, 64.0));
    }
    float SIGMA = max(float(RADIUS), 1.0);
    float TWO_SIGMA2 = 2.0 * SIGMA * SIGMA;

    vec3 blur = vec3(0.0);
    float norm = 0.0;
    for (int x = -RADIUS; x <= RADIUS; x++) {
        for (int y = -RADIUS; y <= RADIUS; y++) {
            float wx = exp(-float(x * x) / TWO_SIGMA2);
            float wy = exp(-float(y * y) / TWO_SIGMA2);
            float weight = wx * wy;
            vec2 offset = texel * vec2(float(x), float(y)) * SPREAD;
            vec3 sample = texture(uTexture, vUV + offset).rgb;
            blur += sample * weight;
            norm += weight;
        }
    }
    blur /= max(norm, 1e-4);

    float coeff = clamp(uCoefficient, 0.0, 1.0);
    // Добавляем только ореол, не трогаем исходные пиксели
    vec3 halo = max(blur - base, 0.0) * coeff;

    FragColor = vec4(halo, 0.0);
}