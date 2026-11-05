#version 330 core
in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uAtlas;
uniform vec4 uColor;
uniform bool uSolid;

void main() {
    if (uSolid) {
        fragColor = uColor;
    } else {
        fragColor = vec4(uColor.rgb, uColor.a * texture(uAtlas, vUV).a);
    }
}
