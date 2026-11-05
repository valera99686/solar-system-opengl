#include "Assets.h"

#include <cstdio>
#include <cstring>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace assets {

namespace {

fs::path g_root;

fs::path executableDirectory() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
    if (n > 0) {
        return fs::path(std::wstring(buffer, n)).parent_path();
    }
#endif
    return fs::current_path();
}

}  // namespace

bool locate() {
    const fs::path exe = executableDirectory();
    const fs::path candidates[] = {exe / "assets", exe.parent_path() / "assets", fs::current_path() / "assets",
                                   fs::current_path().parent_path() / "assets"};
    for (const fs::path& c : candidates) {
        std::error_code ec;
        if (fs::is_directory(c / "shaders", ec)) {
            g_root = c;
            return true;
        }
    }
    return false;
}

void setRoot(const fs::path& root) { g_root = root; }
const fs::path& root() { return g_root; }
fs::path path(const std::string& relative) { return g_root / fs::u8path(relative); }

bool readFile(const fs::path& file, std::vector<unsigned char>& bytes) {
#ifdef _WIN32
    FILE* f = _wfopen(file.c_str(), L"rb");
#else
    FILE* f = std::fopen(file.c_str(), "rb");
#endif
    if (f == nullptr) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    bytes.resize(size > 0 ? static_cast<size_t>(size) : 0);
    const size_t got = bytes.empty() ? 0 : std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return got == bytes.size();
}

bool readText(const fs::path& file, std::string& text) {
    std::vector<unsigned char> bytes;
    if (!readFile(file, bytes)) {
        return false;
    }
    text.assign(bytes.begin(), bytes.end());
    return true;
}

bool writeFile(const fs::path& file, const unsigned char* data, size_t size) {
#ifdef _WIN32
    FILE* f = _wfopen(file.c_str(), L"wb");
#else
    FILE* f = std::fopen(file.c_str(), "wb");
#endif
    if (f == nullptr) {
        return false;
    }
    const size_t put = size == 0 ? 0 : std::fwrite(data, 1, size, f);
    std::fclose(f);
    return put == size;
}

}  // namespace assets

// ---------------------------------------------------------------------------
// TGA
// ---------------------------------------------------------------------------
namespace {

bool fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

}  // namespace

bool decodeTga(const std::vector<unsigned char>& bytes, Image& image, std::string* error) {
    if (bytes.size() < 18) {
        return fail(error, "файл TGA коротший за заголовок");
    }
    const unsigned char idLength = bytes[0];
    const unsigned char colorMapType = bytes[1];
    const unsigned char imageType = bytes[2];
    const int width = bytes[12] | (bytes[13] << 8);
    const int height = bytes[14] | (bytes[15] << 8);
    const int depth = bytes[16];
    const unsigned char descriptor = bytes[17];

    if (colorMapType != 0) {
        return fail(error, "TGA з палітрою не підтримується");
    }
    const bool rle = imageType == 10 || imageType == 11;
    const bool gray = imageType == 3 || imageType == 11;
    if (!(imageType == 2 || imageType == 3 || rle)) {
        return fail(error, "непідтримуваний тип TGA");
    }
    if (width <= 0 || height <= 0) {
        return fail(error, "порожнє зображення TGA");
    }
    if (!(depth == 8 || depth == 24 || depth == 32) || (gray && depth != 8) || (!gray && depth == 8)) {
        return fail(error, "непідтримувана глибина кольору TGA");
    }
    const int channels = depth / 8;
    const size_t pixelCount = static_cast<size_t>(width) * height;
    const size_t bpp = static_cast<size_t>(channels);
    size_t pos = 18 + idLength;

    std::vector<unsigned char> raw(pixelCount * bpp);
    if (!rle) {
        if (pos + raw.size() > bytes.size()) {
            return fail(error, "TGA обірвано");
        }
        std::memcpy(raw.data(), &bytes[pos], raw.size());
    } else {
        size_t out = 0;
        while (out < raw.size()) {
            if (pos >= bytes.size()) {
                return fail(error, "RLE TGA обірвано");
            }
            const unsigned char header = bytes[pos++];
            const size_t count = static_cast<size_t>(header & 0x7F) + 1;
            if (header & 0x80) {
                if (pos + bpp > bytes.size() || out + count * bpp > raw.size()) {
                    return fail(error, "некоректний пакет RLE");
                }
                for (size_t i = 0; i < count; ++i) {
                    std::memcpy(&raw[out], &bytes[pos], bpp);
                    out += bpp;
                }
                pos += bpp;
            } else {
                if (pos + count * bpp > bytes.size() || out + count * bpp > raw.size()) {
                    return fail(error, "некоректний пакет RLE");
                }
                std::memcpy(&raw[out], &bytes[pos], count * bpp);
                out += count * bpp;
                pos += count * bpp;
            }
        }
    }

    // BGR(A) → RGB(A); приведення до порядку «рядки згори вниз»
    const bool topFirst = (descriptor & 0x20) != 0;
    image.width = width;
    image.height = height;
    image.channels = channels;
    image.pixels.assign(raw.size(), 0);
    for (int y = 0; y < height; ++y) {
        const int dstRow = topFirst ? y : height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const unsigned char* src = &raw[(static_cast<size_t>(y) * width + x) * bpp];
            unsigned char* dst = &image.pixels[(static_cast<size_t>(dstRow) * width + x) * bpp];
            if (channels >= 3) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                if (channels == 4) {
                    dst[3] = src[3];
                }
            } else {
                dst[0] = src[0];
            }
        }
    }
    return true;
}

bool loadTga(const std::filesystem::path& file, Image& image, std::string* error) {
    std::vector<unsigned char> bytes;
    if (!assets::readFile(file, bytes)) {
        if (error != nullptr) {
            *error = "не вдалося відкрити файл " + file.u8string();
        }
        return false;
    }
    return decodeTga(bytes, image, error);
}

bool saveTga(const std::filesystem::path& file, const Image& image) {
    if (image.empty() || !(image.channels == 3 || image.channels == 4)) {
        return false;
    }
    std::vector<unsigned char> out(18 + image.pixels.size(), 0);
    out[2] = 2;
    out[12] = static_cast<unsigned char>(image.width & 0xFF);
    out[13] = static_cast<unsigned char>(image.width >> 8);
    out[14] = static_cast<unsigned char>(image.height & 0xFF);
    out[15] = static_cast<unsigned char>(image.height >> 8);
    out[16] = static_cast<unsigned char>(image.channels * 8);
    out[17] = static_cast<unsigned char>(0x20 | (image.channels == 4 ? 8 : 0));   // початок угорі ліворуч
    for (size_t i = 0; i < static_cast<size_t>(image.width) * image.height; ++i) {
        const unsigned char* src = &image.pixels[i * image.channels];
        unsigned char* dst = &out[18 + i * image.channels];
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        if (image.channels == 4) {
            dst[3] = src[3];
        }
    }
    return assets::writeFile(file, out.data(), out.size());
}

Image flippedVertically(const Image& image) {
    Image r = image;
    const size_t row = static_cast<size_t>(image.width) * image.channels;
    for (int y = 0; y < image.height; ++y) {
        std::memcpy(&r.pixels[static_cast<size_t>(image.height - 1 - y) * row],
                    &image.pixels[static_cast<size_t>(y) * row], row);
    }
    return r;
}
