#version 330 core
// Небо: куб, що рухається разом із камерою (з матриці виду прибрано зсув),
// тому зірки здаються нескінченно далекими.
layout(location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vDirection;

void main() {
    vDirection = aPosition;
    mat4 rotationOnly = mat4(mat3(uView));
    gl_Position = uProjection * rotationOnly * vec4(aPosition, 1.0);
}
