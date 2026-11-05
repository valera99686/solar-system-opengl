#version 330 core
// Ореол Сонця: квадрат, повернутий до камери (білборд); матриця моделі містить
// осі камери як стовпці, тому квадрат завжди лежить у площині екрана.
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;

void main() {
    vUV = aUV;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
