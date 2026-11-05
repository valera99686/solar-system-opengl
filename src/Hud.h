#pragma once

// Екранний інтерфейс. Тексти заздалегідь відмальовані скриптом tools/make_assets.py в атлас
// hud_atlas.tga (білі гліфи з альфа-каналом), положення спрайтів — у hud_atlas.txt.
// Програма малює їх текстурованими прямокутниками в піксельній ортогональній проєкції;
// цифри швидкості складаються з гліфів.

#include <map>
#include <string>

#include "GlFuncs.h"
#include "Mesh.h"
#include "Shader.h"

struct HudState {
    bool visible = true;
    bool help = false;
    bool paused = false;
    bool orbitMode = false;          // false — вільний політ, true — слідкування за тілом
    float daysPerSecond = 3.0f;
    std::string selectedId;          // "earth", "ship" … або порожній
};

class Hud {
public:
    bool init(std::string* error);
    void shutdown();
    // Малює елементи інтерфейсу поверх уже готового кадру (width × height пікселів)
    void draw(const HudState& state, int width, int height);

    bool hasSprite(const std::string& name) const { return sprites_.count(name) != 0; }
    // Розмір спрайта в пікселях атласу; (0; 0) для невідомого імені
    gfx::Vec2 spriteSize(const std::string& name) const;

private:
    struct Sprite {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    // Малює спрайт у лівому верхньому куті (px, py); повертає ширину в пікселях екрана
    float drawSprite(const std::string& name, float px, float py, float scale, const gfx::Vec4& color);
    float drawNumber(float value, float px, float py, float scale, const gfx::Vec4& color);
    void drawRect(float px, float py, float w, float h, const gfx::Vec4& color);

    Shader shader_;
    Mesh quad_;
    GLuint atlas_ = 0;
    int atlasWidth_ = 1;
    int atlasHeight_ = 1;
    std::map<std::string, Sprite> sprites_;
    gfx::Mat4 projection_;
};
