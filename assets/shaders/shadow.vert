#version 330 core
// Прохід тіней, крок 1: вершини переводяться у світові координати;
// у простір відсікання кожної з шести граней їх переводить геометричний шейдер.
layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aUV;
layout(location = 4) in mat4 aInstance;

uniform mat4 uModel;
uniform mat4 uBelt;
uniform bool uInstanced;

out vec3 gWorldPos;
out vec2 gUV;

void main() {
    mat4 model = uInstanced ? uBelt * aInstance : uModel;
    vec4 world = model * vec4(aPosition, 1.0);
    gWorldPos = world.xyz;
    gUV = aUV;
    gl_Position = world;
}
