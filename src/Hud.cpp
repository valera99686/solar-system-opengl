#include "Hud.h"

#include <cstdio>
#include <sstream>

#include "Assets.h"
#include "Texture.h"

using namespace gfx;

bool Hud::init(std::string* error) {
    if (!shader_.build("hud.vert", "hud.frag", "", error)) {
        return false;
    }
    quad_.upload(makeQuad());

    Image image;
    if (!loadTga(assets::path("textures/hud_atlas.tga"), image, error)) {
        return false;
    }
    atlasWidth_ = image.width;
    atlasHeight_ = image.height;
    atlas_ = createTexture2D(image, false, false);

    std::string index;
    if (!assets::readText(assets::path("textures/hud_atlas.txt"), index)) {
        if (error != nullptr) {
            *error = "немає файла hud_atlas.txt";
        }
        return false;
    }
    std::istringstream in(index);
    std::string name;
    Sprite s;
    while (in >> name >> s.x >> s.y >> s.w >> s.h) {
        sprites_[name] = s;
    }
    return !sprites_.empty();
}

void Hud::shutdown() {
    if (atlas_ != 0) {
        glDeleteTextures(1, &atlas_);
        atlas_ = 0;
    }
    quad_ = Mesh();
    shader_ = Shader();
}

Vec2 Hud::spriteSize(const std::string& name) const {
    const auto it = sprites_.find(name);
    if (it == sprites_.end()) {
        return {0.0f, 0.0f};
    }
    return {static_cast<float>(it->second.w), static_cast<float>(it->second.h)};
}

float Hud::drawSprite(const std::string& name, float px, float py, float scale, const Vec4& color) {
    const auto it = sprites_.find(name);
    if (it == sprites_.end()) {
        return 0.0f;
    }
    const Sprite& s = it->second;
    const float w = static_cast<float>(s.w) * scale;
    const float h = static_cast<float>(s.h) * scale;
    shader_.set("uSolid", false);
    shader_.set("uRect", Vec4(px, py, w, h));
    shader_.set("uUVRect", Vec4(static_cast<float>(s.x) / atlasWidth_, static_cast<float>(s.y) / atlasHeight_,
                                static_cast<float>(s.x + s.w) / atlasWidth_,
                                static_cast<float>(s.y + s.h) / atlasHeight_));
    shader_.set("uColor", color);
    quad_.draw();
    return w;
}

void Hud::drawRect(float px, float py, float w, float h, const Vec4& color) {
    shader_.set("uSolid", true);
    shader_.set("uRect", Vec4(px, py, w, h));
    shader_.set("uColor", color);
    quad_.draw();
}

float Hud::drawNumber(float value, float px, float py, float scale, const Vec4& color) {
    char text[32];
    std::snprintf(text, sizeof(text), value < 10.0f ? "%.1f" : "%.0f", value);
    float x = px;
    for (const char* c = text; *c != '\0'; ++c) {
        const std::string name = *c == '.' ? "glyph_dot" : std::string("glyph_") + *c;
        x += drawSprite(name, x, py, scale, color);
    }
    return x - px;
}

void Hud::draw(const HudState& state, int width, int height) {
    if (!state.visible || atlas_ == 0) {
        return;
    }
    // Масштаб інтерфейсу залежить від висоти вікна (базова висота — 720 пікселів)
    const float k = clampf(static_cast<float>(height) / 720.0f, 0.75f, 2.5f);
    projection_ = ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, -1.0f, 1.0f);

    GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean cull = glIsEnabled(GL_CULL_FACE);
    GLboolean stencil = glIsEnabled(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.use();
    shader_.set("uProjection", projection_);
    shader_.set("uAtlas", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_);

    const Vec4 white(1.0f, 1.0f, 1.0f, 0.95f);
    const Vec4 accent(1.0f, 0.78f, 0.35f, 1.0f);
    const Vec4 back(0.02f, 0.03f, 0.07f, 0.62f);
    const float m = 12.0f * k;        // відступ від краю вікна
    const float lineH = 26.0f * k;
    const float s = 0.62f * k;        // масштаб спрайтів: у атласі вони намальовані великими

    // Верхня ліва панель: режим, вибраний об'єкт, швидкість
    const float panelW = 300.0f * k;
    const float panelH = lineH * 3.0f + 14.0f * k;
    drawRect(m, m, panelW, panelH, back);
    float y = m + 7.0f * k;
    drawSprite(state.orbitMode ? "mode_orbit" : "mode_free", m + 10.0f * k, y, s, white);
    y += lineH;
    float x = m + 10.0f * k;
    x += drawSprite("selected", x, y, s, white) + 6.0f * k;
    if (!state.selectedId.empty()) {
        drawSprite("body_" + state.selectedId, x, y, s, accent);
    }
    y += lineH;
    x = m + 10.0f * k;
    x += drawSprite("speed", x, y, s, white) + 6.0f * k;
    x += drawNumber(state.daysPerSecond, x, y, s, accent) + 6.0f * k;
    drawSprite("days", x, y, s, white);

    if (state.paused) {
        drawSprite("paused", m + panelW + 10.0f * k, m + 8.0f * k, s * 1.2f, accent);
    }

    // Підказка зі списком клавіш
    if (state.help) {
        const Vec2 hs = spriteSize("help");
        const float hw = hs.x * s;
        const float hh = hs.y * s;
        drawRect(m, static_cast<float>(height) - m - hh - 12.0f * k, hw + 20.0f * k, hh + 12.0f * k, back);
        drawSprite("help", m + 10.0f * k, static_cast<float>(height) - m - hh - 6.0f * k, s, white);
    } else {
        drawSprite("hint", m, static_cast<float>(height) - m - 22.0f * k, s * 0.9f, Vec4(1, 1, 1, 0.7f));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    if (depthTest) {
        glEnable(GL_DEPTH_TEST);
    }
    if (cull) {
        glEnable(GL_CULL_FACE);
    }
    if (stencil) {
        glEnable(GL_STENCIL_TEST);
    }
    if (!blend) {
        glDisable(GL_BLEND);
    }
}
