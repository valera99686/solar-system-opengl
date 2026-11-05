#include "Mesh.h"

#include <cstddef>

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& o) noexcept
    : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_), instanceVbo_(o.instanceVbo_), primitive_(o.primitive_),
      vertexCount_(o.vertexCount_), indexCount_(o.indexCount_), instanceCount_(o.instanceCount_) {
    o.vao_ = o.vbo_ = o.ebo_ = o.instanceVbo_ = 0;
}

Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        destroy();
        vao_ = o.vao_;
        vbo_ = o.vbo_;
        ebo_ = o.ebo_;
        instanceVbo_ = o.instanceVbo_;
        primitive_ = o.primitive_;
        vertexCount_ = o.vertexCount_;
        indexCount_ = o.indexCount_;
        instanceCount_ = o.instanceCount_;
        o.vao_ = o.vbo_ = o.ebo_ = o.instanceVbo_ = 0;
    }
    return *this;
}

void Mesh::destroy() {
    if (instanceVbo_ != 0) {
        glDeleteBuffers(1, &instanceVbo_);
    }
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
    vao_ = vbo_ = ebo_ = instanceVbo_ = 0;
    vertexCount_ = indexCount_ = instanceCount_ = 0;
}

void Mesh::upload(const MeshData& data, GLenum primitive) {
    destroy();
    primitive_ = primitive;
    vertexCount_ = data.vertices.size();
    indexCount_ = data.indices.size();

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(Vertex, tangent)));

    if (!data.indices.empty()) {
        glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);   // прив'язка зберігається у VAO
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned int)),
                     data.indices.data(), GL_STATIC_DRAW);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::setInstances(const std::vector<gfx::Mat4>& matrices) {
    instanceCount_ = matrices.size();
    glBindVertexArray(vao_);
    if (instanceVbo_ == 0) {
        glGenBuffers(1, &instanceVbo_);
    }
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(matrices.size() * sizeof(gfx::Mat4)), matrices.data(),
                 GL_STATIC_DRAW);
    for (int col = 0; col < 4; ++col) {
        const GLuint loc = static_cast<GLuint>(4 + col);
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(gfx::Mat4),
                              reinterpret_cast<const void*>(sizeof(float) * 4 * static_cast<size_t>(col)));
        glVertexAttribDivisor(loc, 1);
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::draw() const {
    glBindVertexArray(vao_);
    if (indexCount_ > 0) {
        glDrawElements(primitive_, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(primitive_, 0, static_cast<GLsizei>(vertexCount_));
    }
    glBindVertexArray(0);
}

void Mesh::drawInstanced() const {
    glBindVertexArray(vao_);
    glDrawElementsInstanced(primitive_, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(instanceCount_));
    glBindVertexArray(0);
}
