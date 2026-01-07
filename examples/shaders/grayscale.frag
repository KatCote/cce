#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uCoefficient;

void main()
{
    vec4 c = texture(uTexture, vUV);
    float g = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    vec3 mixed = mix(c.rgb, vec3(g), clamp(uCoefficient, 0.0, 1.0));
    FragColor = vec4(mixed, c.a);
}
