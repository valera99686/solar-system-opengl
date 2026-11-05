#version 330 core
// Вершинний шейдер планет, супутників, корабля й астероїдів.
// Усі перетворення — множення матриць: clip = P · V · M · v.
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in mat4 aInstance;   // матриця екземпляра (пояс астероїдів), атрибути 4–7

uniform mat4 uModel;
uniform mat3 uNormalMatrix;    // (M₃ₓ₃)^(−T), обчислена на CPU
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uBelt;            // загальне обертання поясу
uniform bool uInstanced;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out mat3 vTBN;                 // дотичний базис для карти нормалей

void main() {
    mat4 model = uInstanced ? uBelt * aInstance : uModel;
    // Для екземплярів матриця нормалей обчислюється тут: масштаб астероїдів нерівномірний
    mat3 nm = uInstanced ? transpose(inverse(mat3(model))) : uNormalMatrix;

    vec4 world = model * vec4(aPosition, 1.0);
    vec3 n = normalize(nm * aNormal);
    vec3 t = normalize(nm * aTangent);
    t = normalize(t - dot(t, n) * n);
    vec3 b = cross(n, t);

    vWorldPos = world.xyz;
    vNormal = n;
    vUV = aUV;
    vTBN = mat3(t, b, n);
    gl_Position = uProjection * uView * world;
}
