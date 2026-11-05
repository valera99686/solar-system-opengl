#include "Renderer.h"

#include <array>
#include <cmath>

#include "Assets.h"
#include "Geometry.h"
#include "Texture.h"

using namespace gfx;

namespace {

// Освітлення сцени
const Vec3 kSunColor(1.0f, 0.93f, 0.82f);
const float kSunIntensity = 3.2f;
const float kSunAttenuation = 0.00032f;     // 1 / (1 + k·d²)
const Vec3 kStarDirection(-0.35f, 0.45f, 0.82f);
const Vec3 kStarColor(0.022f, 0.028f, 0.045f);
const Vec3 kAmbient(0.010f, 0.012f, 0.018f);
const Vec3 kTorchColor(4.0f, 3.8f, 3.4f);

// Напрями й «вгору» шести граней кубічної карти (стандарт OpenGL)
const Vec3 kFaceDirections[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
const Vec3 kFaceUps[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

}  // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(const SolarSystem& system, std::string* error, int shadowSize) {
    struct ShaderSpec {
        Shader* shader;
        const char* vertex;
        const char* fragment;
        const char* geometry;
    };
    const ShaderSpec specs[] = {{&planet_, "planet.vert", "planet.frag", ""},
                                {&sun_, "planet.vert", "sun.frag", ""},
                                {&skybox_, "skybox.vert", "skybox.frag", ""},
                                {&flat_, "flat.vert", "flat.frag", ""},
                                {&shadow_, "shadow.vert", "shadow.frag", "shadow.geom"},
                                {&normals_, "normals.vert", "normals.frag", "normals.geom"},
                                {&glow_, "glow.vert", "glow.frag", ""}};
    for (const ShaderSpec& s : specs) {
        if (!s.shader->build(s.vertex, s.fragment, s.geometry, error)) {
            return false;
        }
    }
    if (!hud_.init(error)) {
        return false;
    }

    // Геометрія
    sphere_.upload(makeSphere(96, 192));
    sphereLow_.upload(makeSphere(14, 28));
    cube_.upload(makeCube());
    quad_.upload(makeQuad());
    circle_.upload(makeCircleLine(192), GL_LINE_LOOP);
    for (int i = 0; i < system.bodyCount(); ++i) {
        const BodyDef& d = system.def(i);
        if (d.ring) {
            ringBody_ = i;
            ring_.upload(makeRing(d.ringInner, d.ringOuter, 160));
        }
    }
    MeshData data;
    if (!loadObj(assets::path("models/ship.obj"), data, error)) {
        return false;
    }
    ship_.upload(data);
    if (!loadObj(assets::path("models/asteroid.obj"), data, error)) {
        return false;
    }
    asteroid_.upload(data);
    asteroid_.setInstances(system.asteroidMatrices());

    // Текстури
    bodyTextures_.assign(static_cast<size_t>(system.bodyCount()), BodyTextures());
    for (int i = 0; i < system.bodyCount(); ++i) {
        const BodyDef& d = system.def(i);
        BodyTextures& t = bodyTextures_[static_cast<size_t>(i)];
        t.diffuse = loadTexture2D(d.id + ".tga", true, error);
        if (t.diffuse == 0) {
            return false;
        }
        if (d.specularMap) {
            t.specular = loadTexture2D(d.id + "_s.tga", false, error);
        }
        if (d.normalMap) {
            t.normal = loadTexture2D(d.id + "_n.tga", false, error);
        }
        if ((d.specularMap && t.specular == 0) || (d.normalMap && t.normal == 0)) {
            return false;
        }
    }
    ringTexture_ = loadTexture2D("saturn_ring.tga", true, error);
    shipTexture_ = loadTexture2D("ship.tga", true, error);
    rockTexture_ = loadTexture2D("rock.tga", true, error);
    sky_ = loadCubemap("sky", true, error);
    if (ringTexture_ == 0 || shipTexture_ == 0 || rockTexture_ == 0 || sky_ == 0) {
        return false;
    }

    if (!createShadowMap(shadowSize, error)) {
        return false;
    }

    // Усі семплери задаємо явно: два семплери різних типів на одному текстурному блоці — помилка
    planet_.use();
    planet_.set("uDiffuse", 0);
    planet_.set("uSpecularMap", 1);
    planet_.set("uNormalMap", 2);
    planet_.set("uShadowMap", 3);
    planet_.set("uEnvironment", 4);
    sun_.use();
    sun_.set("uDiffuse", 0);
    skybox_.use();
    skybox_.set("uSky", 0);
    shadow_.use();
    shadow_.set("uAlphaMap", 0);
    glUseProgram(0);

    ready_ = true;
    return glGetError() == GL_NO_ERROR;
}

void Renderer::shutdown() {
    if (!ready_ && shadowFbo_ == 0) {
        return;
    }
    for (BodyTextures& t : bodyTextures_) {
        GLuint ids[3] = {t.diffuse, t.specular, t.normal};
        for (GLuint id : ids) {
            if (id != 0) {
                glDeleteTextures(1, &id);
            }
        }
    }
    bodyTextures_.clear();
    GLuint singles[5] = {ringTexture_, shipTexture_, rockTexture_, sky_, shadowTexture_};
    for (GLuint id : singles) {
        if (id != 0) {
            glDeleteTextures(1, &id);
        }
    }
    ringTexture_ = shipTexture_ = rockTexture_ = sky_ = shadowTexture_ = 0;
    if (shadowFbo_ != 0) {
        glDeleteFramebuffers(1, &shadowFbo_);
        shadowFbo_ = 0;
    }
    hud_.shutdown();
    sphere_ = Mesh();
    sphereLow_ = Mesh();
    ring_ = Mesh();
    cube_ = Mesh();
    quad_ = Mesh();
    circle_ = Mesh();
    ship_ = Mesh();
    asteroid_ = Mesh();
    planet_ = Shader();
    sun_ = Shader();
    skybox_ = Shader();
    flat_ = Shader();
    shadow_ = Shader();
    normals_ = Shader();
    glow_ = Shader();
    ready_ = false;
}

bool Renderer::createShadowMap(int size, std::string* error) {
    shadowSize_ = size;
    glGenTextures(1, &shadowTexture_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowTexture_);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    // Приєднання всієї кубічної текстури як шаруватої: геометричний шейдер вибирає грань через gl_Layer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    if (!ok && error != nullptr) {
        *error = "буфер кадру карти тіней неповний";
    }
    return ok;
}

void Renderer::bindTexture2D(int unit, GLuint texture) const {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    glBindTexture(GL_TEXTURE_2D, texture);
}

void Renderer::bindCubemap(int unit, GLuint texture) const {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
}

// ---------------------------------------------------------------------------
// Тіні
// ---------------------------------------------------------------------------
void Renderer::renderShadows(const SolarSystem& system) {
    const Vec3 lightPos = system.state(0).position;
    const Mat4 proj = perspective(radians(90.0f), 1.0f, kShadowNear, kShadowFar);
    std::array<Mat4, 6> vp;
    for (int i = 0; i < 6; ++i) {
        vp[static_cast<size_t>(i)] = proj * lookAt(lightPos, lightPos + kFaceDirections[i], kFaceUps[i]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, shadowSize_, shadowSize_);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);   // в карту потрапляють «дальні» межі замкнених тіл — менше самотінення

    shadow_.use();
    shadow_.setArray("uShadowVP", vp.data(), 6);
    shadow_.set("uLightPos", lightPos);
    shadow_.set("uShadowFar", kShadowFar);
    shadow_.set("uAlphaTest", false);
    shadow_.set("uInstanced", false);
    shadow_.set("uBelt", Mat4());

    for (int i = 0; i < system.bodyCount(); ++i) {
        if (system.def(i).star) {
            continue;   // Сонце — саме джерело й тіні не відкидає
        }
        shadow_.set("uModel", system.state(i).model);
        sphere_.draw();
    }

    // Кільце Сатурна: двобічне, прозорі проміжки тінь не відкидають
    if (ringBody_ >= 0) {
        glDisable(GL_CULL_FACE);
        shadow_.set("uAlphaTest", true);
        bindTexture2D(0, ringTexture_);
        shadow_.set("uModel", system.ringModel(ringBody_));
        ring_.draw();
        shadow_.set("uAlphaTest", false);
        glEnable(GL_CULL_FACE);
    }

    shadow_.set("uModel", system.ship().model);
    ship_.draw();

    // Астероїди тіней не відкидають: тисячі дрібних тіл дали б на далеких планетах пістрявий «шум»
    // (і втричі збільшили б вартість проходу), а на кадрі їхні тіні однаково непомітні.

    glCullFace(GL_BACK);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

// ---------------------------------------------------------------------------
// Основний кадр
// ---------------------------------------------------------------------------
void Renderer::setFrameUniforms(const Shader& shader, const SolarSystem& system, const Camera& camera,
                                const RenderSettings& settings) {
    shader.set("uView", camera.view());
    shader.set("uCameraPos", camera.position);
    shader.set("uTexturesOn", settings.textures);
    shader.set("uAmbient", kAmbient);
    shader.set("uSunOn", settings.sunLight);
    shader.set("uSunPos", system.state(0).position);
    shader.set("uSunColor", kSunColor);
    shader.set("uSunIntensity", kSunIntensity);
    shader.set("uSunAttenuation", kSunAttenuation);
    shader.set("uStarDir", normalize(kStarDirection));
    shader.set("uStarColor", kStarColor);
    shader.set("uTorchOn", settings.torch);
    shader.set("uTorchPos", camera.position);
    shader.set("uTorchDir", camera.forward());
    shader.set("uTorchColor", kTorchColor);
    shader.set("uTorchInner", std::cos(radians(9.0f)));
    shader.set("uTorchOuter", std::cos(radians(16.0f)));
    shader.set("uShadowsOn", settings.shadows);
    shader.set("uShadowFar", kShadowFar);
}

void Renderer::setMaterial(const Shader& shader, float shininess, float specular, float reflectivity,
                           const Vec3& atmosphere, float atmosphereStrength) const {
    shader.set("uShininess", shininess);
    shader.set("uSpecularStrength", specular);
    shader.set("uReflectivity", reflectivity);
    shader.set("uAtmosphereColor", atmosphere);
    shader.set("uAtmosphereStrength", atmosphereStrength);
}

void Renderer::drawSky(const Mat4& view, const Mat4& projection) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    skybox_.use();
    skybox_.set("uView", view);
    skybox_.set("uProjection", projection);
    skybox_.set("uBrightness", 1.0f);
    bindCubemap(0, sky_);
    cube_.draw();
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Renderer::drawSun(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                       const Mat4& view, const Mat4& projection) {
    sun_.use();
    sun_.set("uView", view);
    sun_.set("uProjection", projection);
    sun_.set("uCameraPos", camera.position);
    sun_.set("uTexturesOn", settings.textures);
    sun_.set("uEmission", 1.7f);
    sun_.set("uInstanced", false);
    sun_.set("uBelt", Mat4());
    const BodyState& s = system.state(0);
    sun_.set("uModel", s.model);
    sun_.set("uNormalMatrix", normalMatrix(s.model));
    bindTexture2D(0, bodyTextures_[0].diffuse);
    if (settings.selected == 0) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF);
    }
    sphere_.draw();
    glStencilMask(0x00);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::drawBodies(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                          const Mat4& projection) {
    planet_.use();
    planet_.set("uProjection", projection);
    setFrameUniforms(planet_, system, camera, settings);
    planet_.set("uInstanced", false);
    planet_.set("uBelt", Mat4());
    planet_.set("uUseAlpha", false);
    planet_.set("uTwoSided", false);
    bindCubemap(3, shadowTexture_);
    bindCubemap(4, sky_);

    for (int i = 1; i < system.bodyCount(); ++i) {
        const BodyDef& d = system.def(i);
        const BodyState& s = system.state(i);
        const BodyTextures& t = bodyTextures_[static_cast<size_t>(i)];
        planet_.set("uModel", s.model);
        planet_.set("uNormalMatrix", normalMatrix(s.model));
        planet_.set("uHasSpecularMap", d.specularMap);
        planet_.set("uHasNormalMap", d.normalMap);
        setMaterial(planet_, d.shininess, d.specularStrength, 0.0f, d.atmosphere, d.atmosphereStrength);
        bindTexture2D(0, t.diffuse);
        bindTexture2D(1, t.specular);
        bindTexture2D(2, t.normal);
        if (settings.selected == i) {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);
        }
        sphere_.draw();
        glStencilMask(0x00);
    }

    // Корабель: металевий корпус з відбиттям неба
    if (settings.ship) {
        const ShipState& ship = system.ship();
        planet_.set("uModel", ship.model);
        planet_.set("uNormalMatrix", normalMatrix(ship.model));
        planet_.set("uHasSpecularMap", false);
        planet_.set("uHasNormalMap", false);
        setMaterial(planet_, 90.0f, 1.2f, 0.45f, Vec3(), 0.0f);
        bindTexture2D(0, shipTexture_);
        if (settings.selected == system.shipIndex()) {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilMask(0xFF);
        }
        ship_.draw();
        glStencilMask(0x00);
    }

    // Пояс астероїдів: один виклик малює всі екземпляри; кожен має власну матрицю
    if (settings.belt) {
        planet_.set("uInstanced", true);
        planet_.set("uBelt", system.beltRotation());
        planet_.set("uHasSpecularMap", false);
        planet_.set("uHasNormalMap", false);
        setMaterial(planet_, 12.0f, 0.05f, 0.0f, Vec3(), 0.0f);
        bindTexture2D(0, rockTexture_);
        asteroid_.drawInstanced();
        planet_.set("uInstanced", false);
        planet_.set("uBelt", Mat4());
    }

    // Кільце Сатурна: двобічне, напівпрозоре; малюється останнім серед непрозорих об'єктів
    if (ringBody_ >= 0) {
        const Mat4 model = system.ringModel(ringBody_);
        planet_.set("uModel", model);
        planet_.set("uNormalMatrix", normalMatrix(model));
        planet_.set("uUseAlpha", true);
        planet_.set("uTwoSided", true);
        planet_.set("uShadowBiasExtra", 0.9f);   // кільце бачить Сонце під малим кутом: без зсуву воно затінювало б саме себе
        planet_.set("uHasSpecularMap", false);
        planet_.set("uHasNormalMap", false);
        setMaterial(planet_, 8.0f, 0.02f, 0.0f, Vec3(), 0.0f);
        bindTexture2D(0, ringTexture_);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        ring_.draw();
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        planet_.set("uUseAlpha", false);
        planet_.set("uTwoSided", false);
        planet_.set("uShadowBiasExtra", 0.0f);
    }

    glActiveTexture(GL_TEXTURE0);
    for (int unit = 4; unit >= 0; --unit) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
        glBindTexture(unit >= 3 ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawOrbits(const SolarSystem& system, const Mat4& view, const Mat4& projection) {
    flat_.use();
    flat_.set("uView", view);
    flat_.set("uProjection", projection);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (int i = 1; i < system.bodyCount(); ++i) {
        const BodyDef& d = system.def(i);
        flat_.set("uModel", system.state(i).orbitMatrix);
        flat_.set("uColor", d.parent == 0 ? Vec4(0.55f, 0.65f, 0.9f, 0.45f) : Vec4(0.8f, 0.8f, 0.9f, 0.28f));
        circle_.draw();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawGlow(const SolarSystem& system, const Camera& camera, const Mat4& view, const Mat4& projection) {
    // Білборд: матриця повороту взята з осей камери, тому квадрат завжди дивиться в камеру
    const BodyState& sun = system.state(0);
    const float size = system.def(0).radius * 6.0f;
    const Mat4 model = translate(sun.position) * lookRotation(camera.forward(), camera.up()) * scale(size);
    glow_.use();
    glow_.set("uModel", model);
    glow_.set("uView", view);
    glow_.set("uProjection", projection);
    glow_.set("uColor", Vec3(1.0f, 0.72f, 0.36f));
    glow_.set("uStrength", 0.85f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);   // додавання світла
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    quad_.draw();
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::drawOutline(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                           const RenderTarget& target, const Mat4& view, const Mat4& projection) {
    const int sel = settings.selected;
    // Контур: збільшена копія вибраного тіла малюється лише там, де трафарет ≠ 1,
    // тобто поза силуетом самого тіла. Товщина ≈ 3 пікселі незалежно від відстані.
    const Vec3 pos = system.targetPosition(sel);
    const float dist = std::max(length(pos - camera.position), 1e-3f);
    // 1 піксель на відстані dist дорівнює dist / f одиниць світу, f — фокусна відстань у пікселях
    const float focalPx = (static_cast<float>(target.height) * 0.5f) / std::tan(camera.fovY * 0.5f);
    const float widthWorld = 3.0f * (static_cast<float>(target.height) / 720.0f) * dist / focalPx;

    const bool isShip = sel == system.shipIndex();
    const Mat4 base = isShip ? system.ship().model : system.state(sel).model;
    const float modelScale = isShip ? SolarSystem::kShipScale : system.def(sel).radius;
    flat_.use();
    flat_.set("uView", view);
    flat_.set("uProjection", projection);
    flat_.set("uModel", base);
    flat_.set("uExpand", widthWorld / modelScale);
    flat_.set("uColor", Vec4(1.0f, 0.62f, 0.12f, 1.0f));
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilMask(0x00);
    if (isShip) {
        ship_.draw();
    } else {
        sphere_.draw();
    }
    flat_.set("uExpand", 0.0f);
}

void Renderer::drawNormals(const SolarSystem& system, const RenderSettings& settings, const Mat4& view,
                           const Mat4& projection) {
    const int body = (settings.selected >= 0 && settings.selected < system.bodyCount() && settings.selected != 0)
                         ? settings.selected
                         : system.indexOf("earth");
    const BodyState& s = system.state(body);
    normals_.use();
    normals_.set("uModel", s.model);
    normals_.set("uNormalMatrix", normalMatrix(s.model));
    normals_.set("uView", view);
    normals_.set("uProjection", projection);
    normals_.set("uLength", system.def(body).radius * 0.18f);
    normals_.set("uColor", Vec4(0.25f, 1.0f, 0.45f, 1.0f));

    GLuint query = 0;
    glGenQueries(1, &query);
    glBeginQuery(GL_PRIMITIVES_GENERATED, query);   // скільки примітивів (відрізків) створив геометричний шейдер
    sphereLow_.draw();
    glEndQuery(GL_PRIMITIVES_GENERATED);
    GLuint generated = 0;
    glGetQueryObjectuiv(query, GL_QUERY_RESULT, &generated);
    glDeleteQueries(1, &query);
    lastNormalsPrimitives_ = generated;
}

void Renderer::render(const SolarSystem& system, const Camera& camera, const RenderSettings& settings,
                      const RenderTarget& target) {
    if (!ready_) {
        return;
    }
    const float aspect = static_cast<float>(target.width) / static_cast<float>(target.height > 0 ? target.height : 1);
    const Mat4 view = camera.view();
    const Mat4 projection = camera.projection(aspect);

    if (settings.shadows) {
        renderShadows(system);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearStencil(0);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glStencilMask(0x00);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    drawSky(view, projection);
    drawSun(system, camera, settings, view, projection);
    drawBodies(system, camera, settings, projection);
    if (settings.orbits) {
        drawOrbits(system, view, projection);
    }
    if (settings.glow) {
        drawGlow(system, camera, view, projection);
    }
    if (settings.outline && settings.selected >= 0) {
        drawOutline(system, camera, settings, target, view, projection);
    }
    glDisable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    if (settings.normals) {
        drawNormals(system, settings, view, projection);
    }
    glUseProgram(0);
    hud_.draw(settings.hud, target.width, target.height);
    glBindVertexArray(0);
}
