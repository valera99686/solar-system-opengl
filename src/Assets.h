#pragma once

// Доступ до файлів ресурсів (шейдери, текстури, моделі) і зображення в пам'яті.
//
// Каталог assets шукається поруч із виконуваним файлом (bin/..), у каталозі exe та в
// поточному каталозі, тому програму можна запускати з будь-якого місця. Шляхи зберігаються
// як std::filesystem::path (UTF-16 у Windows): у назві каталогу користувача є кирилиця.

#include <filesystem>
#include <string>
#include <vector>

namespace assets {

// Знаходить каталог assets. Повертає false, якщо його немає.
bool locate();
// Явно задає каталог (для тестів)
void setRoot(const std::filesystem::path& root);
const std::filesystem::path& root();
std::filesystem::path path(const std::string& relative);

// Читає файл повністю. Повертає false, якщо файл не відкрився.
bool readFile(const std::filesystem::path& file, std::vector<unsigned char>& bytes);
bool readText(const std::filesystem::path& file, std::string& text);
bool writeFile(const std::filesystem::path& file, const unsigned char* data, size_t size);

}  // namespace assets

// Зображення: рядки йдуть згори вниз, канали в порядку RGB / RGBA / сірий (1 канал)
struct Image {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> pixels;

    bool empty() const { return pixels.empty(); }
    const unsigned char* pixel(int x, int y) const {
        return &pixels[(static_cast<size_t>(y) * width + x) * channels];
    }
};

// Завантажує TGA: типи 2/3 (без стиснення) і 10/11 (RLE), глибина 8, 24 або 32 біти,
// будь-яке розташування початку координат. У разі помилки повертає false й описує причину.
bool loadTga(const std::filesystem::path& file, Image& image, std::string* error = nullptr);
bool decodeTga(const std::vector<unsigned char>& bytes, Image& image, std::string* error = nullptr);
// Записує TGA 24/32 біти без стиснення, рядки згори вниз
bool saveTga(const std::filesystem::path& file, const Image& image);
Image flippedVertically(const Image& image);
