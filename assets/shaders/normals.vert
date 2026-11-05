#version 330 core
// Налагодження нормалей: вершинний шейдер передає світові позицію й нормаль,
// а геометричний перетворює кожну вершину трикутника на відрізок уздовж нормалі.
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 gWorld;
out vec3 gNormal;

void main() {
    gWorld = (uModel * vec4(aPosition, 1.0)).xyz;
    gNormal = normalize(uNormalMatrix * aNormal);
    gl_Position = vec4(gWorld, 1.0);
}
