#pragma once

// Шейдерна програма GLSL 3.30 core: компіляція з файлів assets/shaders, кеш розташувань
// uniform-змінних і типізовані встановлювачі. Геометричний шейдер необов'язковий.

#include <string>
#include <unordered_map>

#include "GlFuncs.h"
#include "Math.h"

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Імена файлів — відносно assets/shaders. geometry може бути порожнім рядком.
    // У разі помилки повертає false, а *error містить журнал компілятора.
    bool build(const std::string& vertex, const std::string& fragment, const std::string& geometry,
               std::string* error);
    // Те саме з вихідного тексту (для тестів)
    bool buildFromSource(const std::string& vertex, const std::string& fragment,
                         const std::string& geometry, std::string* error);

    GLuint id() const { return program_; }
    bool valid() const { return program_ != 0; }
    bool hasGeometryStage() const { return hasGeometry_; }
    void use() const { glUseProgram(program_); }

    GLint location(const char* name) const;   // -1, якщо змінної немає (або її вилучив компілятор)

    void set(const char* name, int value) const { glUniform1i(location(name), value); }
    void set(const char* name, float value) const { glUniform1f(location(name), value); }
    void set(const char* name, bool value) const { glUniform1i(location(name), value ? 1 : 0); }
    void set(const char* name, const gfx::Vec2& v) const { glUniform2f(location(name), v.x, v.y); }
    void set(const char* name, const gfx::Vec3& v) const { glUniform3f(location(name), v.x, v.y, v.z); }
    void set(const char* name, const gfx::Vec4& v) const { glUniform4f(location(name), v.x, v.y, v.z, v.w); }
    void set(const char* name, const gfx::Mat4& m) const {
        glUniformMatrix4fv(location(name), 1, GL_FALSE, m.m);
    }
    void set(const char* name, const gfx::Mat3& m) const {
        glUniformMatrix3fv(location(name), 1, GL_FALSE, m.m);
    }
    // Масив матриць (uShadowVP[6])
    void setArray(const char* name, const gfx::Mat4* matrices, int count) const {
        glUniformMatrix4fv(location(name), count, GL_FALSE, matrices[0].m);
    }

private:
    void destroy();

    GLuint program_ = 0;
    bool hasGeometry_ = false;
    mutable std::unordered_map<std::string, GLint> locations_;
};
