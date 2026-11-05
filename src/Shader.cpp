#include "Shader.h"

#include <vector>

#include "Assets.h"

namespace {

GLuint compile(GLenum type, const std::string& source, const char* label, std::string* error) {
    const GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length) + 1, 0);
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        if (error != nullptr) {
            *error = std::string("помилка компіляції ") + label + ":\n" + log.data();
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Shader::~Shader() { destroy(); }

Shader::Shader(Shader&& other) noexcept
    : program_(other.program_), hasGeometry_(other.hasGeometry_), locations_(std::move(other.locations_)) {
    other.program_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        program_ = other.program_;
        hasGeometry_ = other.hasGeometry_;
        locations_ = std::move(other.locations_);
        other.program_ = 0;
    }
    return *this;
}

void Shader::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    locations_.clear();
}

bool Shader::build(const std::string& vertex, const std::string& fragment, const std::string& geometry,
                   std::string* error) {
    std::string v;
    std::string f;
    std::string g;
    if (!assets::readText(assets::path("shaders/" + vertex), v)) {
        if (error != nullptr) {
            *error = "немає файла шейдера " + vertex;
        }
        return false;
    }
    if (!assets::readText(assets::path("shaders/" + fragment), f)) {
        if (error != nullptr) {
            *error = "немає файла шейдера " + fragment;
        }
        return false;
    }
    if (!geometry.empty() && !assets::readText(assets::path("shaders/" + geometry), g)) {
        if (error != nullptr) {
            *error = "немає файла шейдера " + geometry;
        }
        return false;
    }
    if (!buildFromSource(v, f, g, error)) {
        if (error != nullptr) {
            *error = vertex + " + " + fragment + (geometry.empty() ? "" : " + " + geometry) + ": " + *error;
        }
        return false;
    }
    return true;
}

bool Shader::buildFromSource(const std::string& vertex, const std::string& fragment,
                             const std::string& geometry, std::string* error) {
    destroy();
    const GLuint vs = compile(GL_VERTEX_SHADER, vertex, "вершинного шейдера", error);
    if (vs == 0) {
        return false;
    }
    GLuint gs = 0;
    if (!geometry.empty()) {
        gs = compile(GL_GEOMETRY_SHADER, geometry, "геометричного шейдера", error);
        if (gs == 0) {
            glDeleteShader(vs);
            return false;
        }
    }
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragment, "фрагментного шейдера", error);
    if (fs == 0) {
        glDeleteShader(vs);
        if (gs != 0) {
            glDeleteShader(gs);
        }
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    if (gs != 0) {
        glAttachShader(program, gs);
    }
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (gs != 0) {
        glDeleteShader(gs);
    }

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length) + 1, 0);
        glGetProgramInfoLog(program, length, nullptr, log.data());
        if (error != nullptr) {
            *error = std::string("помилка компонування програми:\n") + log.data();
        }
        glDeleteProgram(program);
        return false;
    }
    program_ = program;
    hasGeometry_ = gs != 0;
    return true;
}

GLint Shader::location(const char* name) const {
    const auto it = locations_.find(name);
    if (it != locations_.end()) {
        return it->second;
    }
    const GLint loc = glGetUniformLocation(program_, name);
    locations_.emplace(name, loc);
    return loc;
}
