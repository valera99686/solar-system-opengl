#include "Texture.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

namespace {

struct Format {
    GLenum internal;
    GLenum external;
};

Format formatFor(int channels, bool srgb) {
    switch (channels) {
        case 1: return {GL_R8, GL_RED};
        case 3: return {static_cast<GLenum>(srgb ? GL_SRGB8 : GL_RGB8), GL_RGB};
        default: return {static_cast<GLenum>(srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8), GL_RGBA};
    }
}

void setAnisotropy() {
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (glGetError() != GL_NO_ERROR) {
        return;   // розширення недоступне — ігноруємо
    }
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso < 8.0f ? maxAniso : 8.0f);
}

}  // namespace

GLuint createTexture2D(const Image& image, bool srgb, bool repeatS) {
    if (image.empty()) {
        return 0;
    }
    // Рядок TGA/зображення йде згори вниз, а OpenGL очікує нижній рядок першим
    const Image flipped = flippedVertically(image);
    const Format f = formatFor(image.channels, srgb);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(f.internal), image.width, image.height, 0, f.external,
                 GL_UNSIGNED_BYTE, flipped.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeatS ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    setAnisotropy();
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createCubemap(const std::array<Image, 6>& faces, bool srgb) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int i = 0; i < 6; ++i) {
        const Image& img = faces[static_cast<size_t>(i)];
        if (img.empty()) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glDeleteTextures(1, &tex);
            return 0;
        }
        const Format f = formatFor(img.channels, srgb);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, static_cast<GLint>(f.internal), img.width, img.height,
                     0, f.external, GL_UNSIGNED_BYTE, img.pixels.data());
    }
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return tex;
}

GLuint loadTexture2D(const std::string& name, bool srgb, std::string* error) {
    Image image;
    if (!loadTga(assets::path("textures/" + name), image, error)) {
        return 0;
    }
    return createTexture2D(image, srgb);
}

GLuint loadCubemap(const std::string& prefix, bool srgb, std::string* error) {
    static const char* suffix[6] = {"px", "nx", "py", "ny", "pz", "nz"};
    std::array<Image, 6> faces;
    for (int i = 0; i < 6; ++i) {
        if (!loadTga(assets::path("textures/" + prefix + "_" + suffix[i] + ".tga"), faces[static_cast<size_t>(i)],
                     error)) {
            return 0;
        }
    }
    return createCubemap(faces, srgb);
}
