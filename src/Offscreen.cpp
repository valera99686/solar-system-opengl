#include "Offscreen.h"

#include <cstring>

Offscreen::~Offscreen() { destroy(); }

void Offscreen::destroy() {
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
    }
    if (resolveFbo_ != 0) {
        glDeleteFramebuffers(1, &resolveFbo_);
    }
    if (colorRb_ != 0) {
        glDeleteRenderbuffers(1, &colorRb_);
    }
    if (depthRb_ != 0) {
        glDeleteRenderbuffers(1, &depthRb_);
    }
    if (resolveRb_ != 0) {
        glDeleteRenderbuffers(1, &resolveRb_);
    }
    fbo_ = resolveFbo_ = colorRb_ = depthRb_ = resolveRb_ = 0;
    width_ = height_ = 0;
}

bool Offscreen::create(int width, int height, int samples) {
    destroy();
    width_ = width;
    height_ = height;
    samples_ = samples < 1 ? 1 : samples;

    glGenRenderbuffers(1, &colorRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA8, width, height);
    glGenRenderbuffers(1, &depthRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH24_STENCIL8, width, height);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRb_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRb_);
    const bool mainOk = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glGenRenderbuffers(1, &resolveRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, resolveRb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glGenFramebuffers(1, &resolveFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolveRb_);
    const bool resolveOk = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!(mainOk && resolveOk)) {
        destroy();
        return false;
    }
    return true;
}

Image Offscreen::read() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    Image image;
    image.width = width_;
    image.height = height_;
    image.channels = 3;
    image.pixels.assign(static_cast<size_t>(width_) * height_ * 3, 0);
    std::vector<unsigned char> rgba(static_cast<size_t>(width_) * height_ * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFbo_);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    // OpenGL віддає нижній рядок першим — перевертаємо й відкидаємо альфа-канал
    for (int y = 0; y < height_; ++y) {
        const unsigned char* src = &rgba[static_cast<size_t>(height_ - 1 - y) * width_ * 4];
        unsigned char* dst = &image.pixels[static_cast<size_t>(y) * width_ * 3];
        for (int x = 0; x < width_; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 0];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 2];
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return image;
}
