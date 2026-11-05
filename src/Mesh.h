#pragma once

// Меш на GPU: VAO + вершинний буфер (+ індексний) і необов'язковий буфер матриць екземплярів.
//
// Атрибути вершини: 0 позиція, 1 нормаль, 2 UV, 3 дотичний вектор.
// Матриця екземпляра mat4 займає чотири атрибути 4–7 (по стовпцю) з дільником 1.

#include <vector>

#include "Geometry.h"
#include "GlFuncs.h"

class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void upload(const MeshData& data, GLenum primitive = GL_TRIANGLES);
    // Завантажує матриці екземплярів (по одній на примірник) в атрибути 4–7
    void setInstances(const std::vector<gfx::Mat4>& matrices);

    void draw() const;
    void drawInstanced() const;   // instanceCount() примірників за один виклик

    bool valid() const { return vao_ != 0; }
    size_t vertexCount() const { return vertexCount_; }
    size_t indexCount() const { return indexCount_; }
    size_t instanceCount() const { return instanceCount_; }

private:
    void destroy();

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint instanceVbo_ = 0;
    GLenum primitive_ = GL_TRIANGLES;
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;
    size_t instanceCount_ = 0;
};
