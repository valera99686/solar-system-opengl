#version 330 core
layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uLength;

in vec3 gWorld[];
in vec3 gNormal[];

void main() {
    for (int i = 0; i < 3; ++i) {
        gl_Position = uProjection * uView * vec4(gWorld[i], 1.0);
        EmitVertex();
        gl_Position = uProjection * uView * vec4(gWorld[i] + gNormal[i] * uLength, 1.0);
        EmitVertex();
        EndPrimitive();
    }
}
