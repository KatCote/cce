#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uTint;
uniform float uCoefficient;

void main()
{
    vec4 c = texture(uTexture, vUV);
    float coeff = clamp(uCoefficient, 0.0, 1.0);
    vec3 tinted = c.rgb * uTint.rgb;
    vec3 mixed = mix(c.rgb, tinted, coeff);
    float alpha = c.a * mix(1.0, uTint.a, coeff);
    FragColor = vec4(mixed, alpha);
}
