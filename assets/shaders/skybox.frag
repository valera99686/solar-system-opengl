#version 330 core
in vec3 vDirection;

out vec4 fragColor;

uniform samplerCube uSky;
uniform float uBrightness;

void main() {
    vec3 color = texture(uSky, vDirection).rgb * uBrightness;
    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
