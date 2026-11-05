#version 330 core
// Екранний інтерфейс: прямокутник у пікселях (початок координат — лівий верхній кут).
layout(location = 0) in vec3 aPosition;

uniform mat4 uProjection;   // ортогональна: (0, W) × (H, 0)
uniform vec4 uRect;         // x, y, ширина, висота у пікселях
uniform vec4 uUVRect;       // u0, v0, u1, v1 (v рахується згори вниз)

out vec2 vUV;

void main() {
    vec2 t = aPosition.xy * 0.5 + 0.5;
    vec2 pixel = uRect.xy + vec2(t.x, 1.0 - t.y) * uRect.zw;
    float vImage = mix(uUVRect.w, uUVRect.y, t.y);
    vUV = vec2(mix(uUVRect.x, uUVRect.z, t.x), 1.0 - vImage);
    gl_Position = uProjection * vec4(pixel, 0.0, 1.0);
}
