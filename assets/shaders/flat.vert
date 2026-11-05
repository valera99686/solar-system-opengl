#version 330 core
// Однокольорові об'єкти: орбіти, контур виділення.
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uExpand;    // зміщення вершин уздовж нормалі (одиниці моделі): товщина контуру

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPosition + aNormal * uExpand, 1.0);
}
