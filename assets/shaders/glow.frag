#version 330 core
in vec2 vUV;

out vec4 fragColor;

uniform vec3 uColor;
uniform float uStrength;

void main() {
    float d = length(vUV * 2.0 - 1.0);
    float halo = pow(clamp(1.0 - d, 0.0, 1.0), 2.4);
    float core = pow(clamp(1.0 - d * 2.4, 0.0, 1.0), 1.5);
    fragColor = vec4(uColor * (halo * 0.9 + core * 0.6) * uStrength, 1.0);
}
