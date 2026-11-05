#pragma once

// Позаекранний кадр: багатовибірковий (MSAA) буфер кольору та глибини зі стенсилом плюс буфер
// розв'язування. Дає змогу відтворювати кадри будь-якого розміру незалежно від вікна — для
// знімків екрана у звіті та для автоматичних перевірок зображення.

#include "Assets.h"
#include "GlFuncs.h"

class Offscreen {
public:
    Offscreen() = default;
    ~Offscreen();
    Offscreen(const Offscreen&) = delete;
    Offscreen& operator=(const Offscreen&) = delete;

    // samples = 1 — без згладжування. Повертає false, якщо буфер кадру неповний.
    bool create(int width, int height, int samples);
    void destroy();

    GLuint framebuffer() const { return fbo_; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Розв'язує вибірки й читає кадр як зображення RGB (рядки згори вниз)
    Image read();

private:
    GLuint fbo_ = 0;
    GLuint colorRb_ = 0;
    GLuint depthRb_ = 0;
    GLuint resolveFbo_ = 0;
    GLuint resolveRb_ = 0;
    int width_ = 0;
    int height_ = 0;
    int samples_ = 1;
};
