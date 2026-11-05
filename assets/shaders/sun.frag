#version 330 core
// Сонце світиться саме: колір текстури, помножений на яскравість, із потемнінням до краю диска.
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uDiffuse;
uniform bool uTexturesOn;
uniform vec3 uCameraPos;
uniform float uEmission;

void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCameraPos - vWorldPos);
    float mu = max(dot(n, v), 0.0);
    float limb = 0.55 + 0.45 * pow(mu, 0.6);
    vec3 base = uTexturesOn ? texture(uDiffuse, vUV).rgb : vec3(1.0, 0.7, 0.3);
    vec3 color = base * uEmission * limb;
    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
