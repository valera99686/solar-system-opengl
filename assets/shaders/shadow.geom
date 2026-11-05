#version 330 core
// Прохід тіней, крок 2: КОЖЕН трикутник за один виклик малюється в усі шість граней кубічної
// карти глибини. Вбудована змінна gl_Layer вибирає грань шару (layered rendering),
// тож сцена обробляється один раз замість шести окремих проходів.
layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform mat4 uShadowVP[6];   // P · V для граней +X, −X, +Y, −Y, +Z, −Z

in vec3 gWorldPos[];
in vec2 gUV[];

out vec3 fWorldPos;
out vec2 fUV;

void main() {
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int i = 0; i < 3; ++i) {
            fWorldPos = gWorldPos[i];
            fUV = gUV[i];
            gl_Position = uShadowVP[face] * vec4(fWorldPos, 1.0);
            EmitVertex();
        }
        EndPrimitive();
    }
}
