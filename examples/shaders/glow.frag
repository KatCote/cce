#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uResolution;
uniform float uCoefficient;

void main()
{
    vec2 texel = 1.0 / uResolution;
    vec4 sum = vec4(0.0);
    float weights[5] = float[](0.204164, 0.304005, 0.093913, 0.01856, 0.004429);

    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            float w = weights[abs(x)] * weights[abs(y)];
            sum += texture(uTexture, vUV + vec2(x, y) * texel) * w;
        }
    }

    vec4 base = texture(uTexture, vUV);
    float coeff = clamp(uCoefficient, 0.0, 2.5);
    vec3 glow = sum.rgb * coeff;
    vec3 finalColor = base.rgb + glow;
    float alpha = max(base.a, sum.a * min(coeff, 1.0));

    FragColor = vec4(finalColor, alpha);
}
