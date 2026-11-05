#pragma once

// Створення текстур OpenGL із зображень TGA: звичайні 2D (з міпрівнями й анізотропією)
// та кубічні карти (для неба).

#include <array>
#include <string>

#include "Assets.h"
#include "GlFuncs.h"

// sRGB = true для кольорових карт (відтінки, що їх бачить око): GPU перетворює їх у лінійний
// простір під час вибірки, тож освітлення рахується коректно. Для даних (спекулярність,
// нормалі) sRGB = false.
GLuint createTexture2D(const Image& image, bool srgb, bool repeatS = true);

// Грані кубічної карти в порядку +X, −X, +Y, −Y, +Z, −Z; рядки згори вниз, як в API OpenGL
GLuint createCubemap(const std::array<Image, 6>& faces, bool srgb);

// Завантажує текстуру з assets/textures/<name>; повертає 0 і заповнює *error за невдачі
GLuint loadTexture2D(const std::string& name, bool srgb, std::string* error);
GLuint loadCubemap(const std::string& prefix, bool srgb, std::string* error);   // prefix_px.tga …
