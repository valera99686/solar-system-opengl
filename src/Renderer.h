#pragma once

// Відтворення сцени. Один кадр складається з проходів:
//   1) тіні: сцена малюється з Сонця в кубічну карту глибини за один виклик
//      (геометричний шейдер розсилає трикутники у шість граней);
//   2) небо (кубічна карта);
//   3) Сонце, планети й супутники, корабель, пояс астероїдів (інстансинг), кільце Сатурна;
//   4) орбіти та ореол Сонця;
//   5) контур вибраного тіла (тест трафарету), нормалі (геометричний шейдер), екранний інтерфейс.

#include <string>
#include <vector>

#include "Camera.h"
#include "Hud.h"
#include "Mesh.h"
#include "Shader.h"
#include "SolarSystem.h"

struct RenderSettings {
    bool shadows = true;
    bool orbits = true;
    bool belt = true;
    bool ship = true;
    bool torch = false;        // прожектор камери
    bool normals = false;      // показ нормалей геометричним шейдером
    bool outline = true;       // контур вибраного тіла (трафарет)
    bool glow = true;          // ореол Сонця
    bool textures = true;
    bool sunLight = true;
    int selected = -1;         // індекс вибраного об'єкта або −1
    HudState hud;
};

struct RenderTarget {
    GLuint framebuffer = 0;    // 0 — вікно
    int width = 1;
    int height = 1;
};

class Renderer {
public:
    static constexpr float kShadowFar = 400.0f;
    static constexpr float kShadowNear = 0.5f;

    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Завантажує шейдери, текстури, моделі; створює карту тіней shadowSize × shadowSize на грань
    bool init(const SolarSystem& system, std::string* error, int shadowSize = 2048);
    void shutdown();

    void render(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                const RenderTarget& target);

    // Для перевірок
    int shadowSize() const { return shadowSize_; }
    GLuint shadowTexture() const { return shadowTexture_; }
    const Shader& planetShader() const { return planet_; }
    const Shader& shadowShader() const { return shadow_; }
    const Shader& normalsShader() const { return normals_; }
    Mesh& sphereMesh() { return sphere_; }
    // Кількість трикутників, оброблених геометричним шейдером останнього проходу нормалей
    unsigned int lastNormalsPrimitives() const { return lastNormalsPrimitives_; }
    // Підміна кубічної карти неба (для перевірки відповідності граней); повертає попередню
    GLuint replaceSky(GLuint texture) {
        const GLuint old = sky_;
        sky_ = texture;
        return old;
    }
    size_t sphereLowTriangles() const { return sphereLow_.indexCount() / 3; }
    size_t beltInstances() const { return asteroid_.instanceCount(); }

private:
    struct BodyTextures {
        GLuint diffuse = 0;
        GLuint specular = 0;
        GLuint normal = 0;
    };

    bool createShadowMap(int size, std::string* error);
    void renderShadows(const SolarSystem& system);
    void setFrameUniforms(const Shader& shader, const SolarSystem& system, const Camera& camera,
                          const RenderSettings& settings);
    void drawSky(const gfx::Mat4& view, const gfx::Mat4& projection);
    void drawBodies(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                    const gfx::Mat4& projection);
    void drawSun(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                 const gfx::Mat4& view, const gfx::Mat4& projection);
    void drawOrbits(const SolarSystem& system, const gfx::Mat4& view, const gfx::Mat4& projection);
    void drawGlow(const SolarSystem& system, const Camera& camera, const gfx::Mat4& view,
                  const gfx::Mat4& projection);
    void drawOutline(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                     const RenderTarget& target, const gfx::Mat4& view, const gfx::Mat4& projection);
    void drawNormals(const SolarSystem& system, const RenderSettings& settings, const gfx::Mat4& view,
                     const gfx::Mat4& projection);
    void bindTexture2D(int unit, GLuint texture) const;
    void bindCubemap(int unit, GLuint texture) const;
    // Параметри матеріалу тіла; для корабля й астероїдів задаються окремо
    void setMaterial(const Shader& shader, float shininess, float specular, float reflectivity,
                     const gfx::Vec3& atmosphere, float atmosphereStrength) const;

    Shader planet_;
    Shader sun_;
    Shader skybox_;
    Shader flat_;
    Shader shadow_;
    Shader normals_;
    Shader glow_;
    Hud hud_;

    Mesh sphere_;
    Mesh sphereLow_;
    Mesh ring_;
    Mesh cube_;
    Mesh quad_;
    Mesh circle_;
    Mesh ship_;
    Mesh asteroid_;

    std::vector<BodyTextures> bodyTextures_;
    GLuint ringTexture_ = 0;
    GLuint shipTexture_ = 0;
    GLuint rockTexture_ = 0;
    GLuint sky_ = 0;

    int ringBody_ = -1;
    int shadowSize_ = 0;
    GLuint shadowFbo_ = 0;
    GLuint shadowTexture_ = 0;
    unsigned int lastNormalsPrimitives_ = 0;
    bool ready_ = false;
};
