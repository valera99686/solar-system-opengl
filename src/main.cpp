// Лабораторна робота №3 з комп'ютерної графіки: модель Сонячної системи (OpenGL 3.3 core, GLFW).
//
// Інтерактивний режим: політ камерою, вибір тіл, керування часом і ефектами.
// Режим знімка (--shot файл.tga) малює один кадр у позаекранний буфер і виходить —
// так створюються ілюстрації до звіту.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "GlFuncs.h"

#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include "Assets.h"
#include "Camera.h"
#include "Offscreen.h"
#include "Presets.h"
#include "Renderer.h"
#include "SolarSystem.h"

using namespace gfx;

namespace {

struct Options {
    std::string shotFile;
    std::string preset;
    std::string select;
    bool hasTime = false;
    double days = 0.0;
    int width = 1280;
    int height = 720;
    int samples = 4;
    int shadowSize = 2048;
    bool noShadows = false;
    bool noOrbits = false;
    bool noHud = false;
    bool noBelt = false;
    bool noTextures = false;
    bool noOutline = false;
    bool noSunLight = false;
    bool torch = false;
    bool normals = false;
    bool hudHelp = false;
    bool help = false;
    bool listPresets = false;
    std::string dumpBodies;     // --dump-bodies ФАЙЛ
    bool hasCamera = false;     // --camera ex,ey,ez,tx,ty,tz
    float cameraEye[3] = {0.0f, 0.0f, 0.0f};
    float cameraTarget[3] = {0.0f, 0.0f, 0.0f};
    float fovDegrees = 0.0f;    // --fov
    float rollDegrees = 0.0f;   // --roll
};

bool g_showDialogs = true;   // у режимі знімка діалоги не потрібні: програма працює без людини

void showError(const std::string& text) {
    std::fprintf(stderr, "%s\n", text.c_str());
#ifdef _WIN32
    if (!g_showDialogs) {
        return;
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], n);
    MessageBoxW(nullptr, wide.c_str(), L"ЛР3 КГ", MB_OK | MB_ICONERROR);
#endif
}

bool parseArguments(const std::vector<std::string>& args, Options& o) {
    const int argc = static_cast<int>(args.size());
    for (int i = 1; i < argc; ++i) {
        const std::string a = args[static_cast<size_t>(i)];
        auto next = [&](std::string& out) {
            if (i + 1 >= argc) {
                return false;
            }
            out = args[static_cast<size_t>(++i)];
            return true;
        };
        std::string v;
        if (a == "--shot" && next(o.shotFile)) {
        } else if (a == "--view" && next(o.preset)) {
        } else if (a == "--select" && next(o.select)) {
        } else if (a == "--time" && next(v)) {
            o.days = std::atof(v.c_str());
            o.hasTime = true;
        } else if (a == "--size" && next(v)) {
            if (std::sscanf(v.c_str(), "%dx%d", &o.width, &o.height) != 2 || o.width < 16 || o.height < 16) {
                return false;
            }
        } else if (a == "--samples" && next(v)) {
            o.samples = std::max(1, std::atoi(v.c_str()));
        } else if (a == "--shadow-size" && next(v)) {
            o.shadowSize = std::max(64, std::atoi(v.c_str()));
        } else if (a == "--camera" && next(v)) {
            if (std::sscanf(v.c_str(), "%f,%f,%f,%f,%f,%f", &o.cameraEye[0], &o.cameraEye[1], &o.cameraEye[2],
                            &o.cameraTarget[0], &o.cameraTarget[1], &o.cameraTarget[2]) != 6) {
                return false;
            }
            o.hasCamera = true;
        } else if (a == "--fov" && next(v)) {
            o.fovDegrees = static_cast<float>(std::atof(v.c_str()));
        } else if (a == "--roll" && next(v)) {
            o.rollDegrees = static_cast<float>(std::atof(v.c_str()));
        } else if (a == "--no-shadows") {
            o.noShadows = true;
        } else if (a == "--no-orbits") {
            o.noOrbits = true;
        } else if (a == "--no-hud") {
            o.noHud = true;
        } else if (a == "--no-belt") {
            o.noBelt = true;
        } else if (a == "--no-textures") {
            o.noTextures = true;
        } else if (a == "--no-outline") {
            o.noOutline = true;
        } else if (a == "--no-sun-light") {
            o.noSunLight = true;
        } else if (a == "--torch") {
            o.torch = true;
        } else if (a == "--normals") {
            o.normals = true;
        } else if (a == "--hud-help") {
            o.hudHelp = true;
        } else if (a == "--help" || a == "-h") {
            o.help = true;
        } else if (a == "--dump-bodies" && next(o.dumpBodies)) {
        } else if (a == "--list-views") {
            o.listPresets = true;
        } else {
            return false;
        }
    }
    return true;
}

const char* kUsage =
    "Використання: scene3d [параметри]\n"
    "  --shot ФАЙЛ.tga   відтворити один кадр у файл і вийти (без вікна)\n"
    "  --view ІМ'Я       ракурс камери (див. --list-views)\n"
    "  --time ДОБИ       момент часу, діб від початку відліку\n"
    "  --select ІД       тіло для контуру (sun, earth, saturn, ship …)\n"
    "  --size ШxВ        розмір кадру (за замовчуванням 1280x720)\n"
    "  --samples N       згладжування MSAA (за замовчуванням 4)\n"
    "  --shadow-size N   сторона грані карти тіней (за замовчуванням 2048)\n"
    "  --camera ex,ey,ez,tx,ty,tz   поставити камеру в точку e, що дивиться на t\n"
    "  --fov ГРАДУСИ     вертикальний кут огляду\n"
    "  --roll ГРАДУСИ    крен камери навколо напрямку погляду\n"
    "  --no-shadows --no-orbits --no-hud --no-belt --no-textures --no-outline --no-sun-light\n"
    "  --torch --normals --hud-help\n";

// Ідентифікатори тіл для клавіш 1…0
const char* kNumberKeys[10] = {"sun", "mercury", "venus", "earth", "mars", "jupiter", "saturn", "uranus", "neptune", "ship"};

class App {
public:
    GLFWwindow* window = nullptr;
    SolarSystem system;
    Renderer renderer;
    Camera camera;
    RenderSettings settings;

    double days = 0.0;
    float daysPerSecond = 3.0f;
    bool paused = false;

    bool orbitMode = false;
    int orbitTarget = 3;
    float orbitDistance = 12.0f;
    float orbitAzimuth = 0.0f;
    float orbitElevation = 0.3f;

    int speedLevel = 0;            // швидкість польоту = 30 · 1,25^level
    bool looking = false;
    bool haveLast = false;
    double lastX = 0.0;
    double lastY = 0.0;
    bool wantScreenshot = false;
    int screenshotCounter = 0;

    void applyOptions(const Options& o) {
        settings.shadows = !o.noShadows;
        settings.orbits = !o.noOrbits;
        settings.hud.visible = !o.noHud;
        settings.belt = !o.noBelt;
        settings.textures = !o.noTextures;
        settings.outline = !o.noOutline;
        settings.sunLight = !o.noSunLight;
        settings.torch = o.torch;
        settings.normals = o.normals;
        settings.hud.help = o.hudHelp;
    }

    void selectTarget(int index) {
        settings.selected = index;
        settings.hud.selectedId = index < 0 ? "" : idOf(index);
    }

    std::string idOf(int index) const { return index == system.shipIndex() ? "ship" : system.def(index).id; }

    int indexOfId(const std::string& id) const {
        if (id == "ship") {
            return system.shipIndex();
        }
        return system.indexOf(id);
    }

    // Переходить в орбітальний режим навколо вибраного об'єкта, зберігаючи поточний ракурс
    void enterOrbitMode(int index, bool keepView) {
        orbitMode = true;
        orbitTarget = index;
        const Vec3 target = system.targetPosition(index);
        const Vec3 offset = camera.position - target;
        const float dist = length(offset);
        const float minDist = system.targetRadius(index) * 1.8f;
        if (keepView && dist > minDist && dist < 400.0f) {
            orbitDistance = dist;
            orbitAzimuth = std::atan2(offset.z, offset.x);
            orbitElevation = std::asin(clampf(offset.y / dist, -1.0f, 1.0f));
        } else {
            orbitDistance = std::max(system.targetRadius(index) * 5.0f, 2.5f);
        }
    }

    void leaveOrbitMode() { orbitMode = false; }

    void resetView() {
        const ViewPreset* p = findPreset("overview");
        applyPreset(*p, system, camera);
        orbitMode = false;
        selectTarget(-1);
    }

    void onKey(int key, int action, int mods) {
        if (action != GLFW_PRESS) {
            return;
        }
        switch (key) {
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
            case GLFW_KEY_P: paused = !paused; break;
            case GLFW_KEY_COMMA: daysPerSecond = std::max(0.05f, daysPerSecond / 1.5f); break;
            case GLFW_KEY_PERIOD: daysPerSecond = std::min(400.0f, daysPerSecond * 1.5f); break;
            case GLFW_KEY_T: settings.textures = !settings.textures; break;
            case GLFW_KEY_G: settings.shadows = !settings.shadows; break;
            case GLFW_KEY_O: settings.orbits = !settings.orbits; break;
            case GLFW_KEY_B: settings.belt = !settings.belt; break;
            case GLFW_KEY_F: settings.torch = !settings.torch; break;
            case GLFW_KEY_N: settings.normals = !settings.normals; break;
            case GLFW_KEY_L: settings.sunLight = !settings.sunLight; break;
            case GLFW_KEY_X: settings.outline = !settings.outline; break;
            case GLFW_KEY_H: settings.hud.help = !settings.hud.help; break;
            case GLFW_KEY_I: settings.hud.visible = !settings.hud.visible; break;
            case GLFW_KEY_R: resetView(); break;
            case GLFW_KEY_F12: wantScreenshot = true; break;
            case GLFW_KEY_C:
                if (orbitMode) {
                    leaveOrbitMode();
                } else if (settings.selected >= 0) {
                    enterOrbitMode(settings.selected, true);
                }
                break;
            case GLFW_KEY_TAB: {
                const int n = system.selectableCount();
                const int step = (mods & GLFW_MOD_SHIFT) ? n - 1 : 1;
                const int next = settings.selected < 0 ? ((mods & GLFW_MOD_SHIFT) ? n - 1 : 0)
                                                       : (settings.selected + step) % n;
                selectTarget(next);
                if (orbitMode) {
                    enterOrbitMode(next, false);
                }
                break;
            }
            default:
                break;
        }
        // Цифри: вибрати тіло й летіти до нього (орбітальний режим)
        int digit = -1;
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            digit = key - GLFW_KEY_1;
        } else if (key == GLFW_KEY_0) {
            digit = 9;
        }
        if (digit >= 0) {
            const int idx = indexOfId(kNumberKeys[digit]);
            if (idx >= 0) {
                selectTarget(idx);
                enterOrbitMode(idx, false);
            }
        }
    }

    void onMouseButton(int button, int action) {
        if (button != GLFW_MOUSE_BUTTON_RIGHT) {
            return;
        }
        looking = action == GLFW_PRESS;
        glfwSetInputMode(window, GLFW_CURSOR, looking ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        haveLast = false;
    }

    void onCursor(double x, double y) {
        if (!looking) {
            return;
        }
        if (haveLast) {
            const float dx = static_cast<float>(x - lastX);
            const float dy = static_cast<float>(y - lastY);
            const float sensitivity = 0.0024f;
            if (orbitMode) {
                orbitAzimuth -= dx * sensitivity;
                orbitElevation = clampf(orbitElevation + dy * sensitivity, radians(-89.0f), radians(89.0f));
            } else {
                camera.rotateLocal(-dx * sensitivity, -dy * sensitivity, 0.0f);
            }
        }
        lastX = x;
        lastY = y;
        haveLast = true;
    }

    void onScroll(double dy) {
        if (orbitMode) {
            orbitDistance = clampf(orbitDistance * std::pow(0.88f, static_cast<float>(dy)),
                                   system.targetRadius(orbitTarget) * 1.4f, 600.0f);
        } else {
            speedLevel = std::max(-12, std::min(14, speedLevel + static_cast<int>(dy)));
        }
    }

    void updateCamera(float dt) {
        if (orbitMode) {
            camera.orbit(system.targetPosition(orbitTarget), orbitDistance, orbitAzimuth, orbitElevation);
            return;
        }
        const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        const float speed = 30.0f * std::pow(1.25f, static_cast<float>(speedLevel)) * (shift ? 5.0f : 1.0f) * dt;
        Vec3 move;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move.z += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move.z -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move.x += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move.x -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) move.y += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) move.y -= 1.0f;
        camera.moveLocal(move * speed);
        float roll = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) roll += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) roll -= 1.0f;
        if (roll != 0.0f) {
            camera.rotateLocal(0.0f, 0.0f, roll * 1.2f * dt);
        }
    }

    std::string title(double fps) const {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "ЛР3 КГ · Сонячна система · %.0f к/с · %s · день %.0f", fps,
                      orbitMode ? "орбітальна камера" : "вільна камера", days);
        return buf;
    }
};

App* appFrom(GLFWwindow* w) { return static_cast<App*>(glfwGetWindowUserPointer(w)); }

void keyCallback(GLFWwindow* w, int key, int, int action, int mods) { appFrom(w)->onKey(key, action, mods); }
void mouseButtonCallback(GLFWwindow* w, int button, int action, int) { appFrom(w)->onMouseButton(button, action); }
void cursorCallback(GLFWwindow* w, double x, double y) { appFrom(w)->onCursor(x, y); }
void scrollCallback(GLFWwindow* w, double, double dy) { appFrom(w)->onScroll(dy); }

bool createContext(App& app, const Options& o, bool visible) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_SAMPLES, visible ? 4 : 0);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
    app.window = glfwCreateWindow(visible ? o.width : 64, visible ? o.height : 64, "ЛР3 КГ · Сонячна система",
                                  nullptr, nullptr);
    if (app.window == nullptr) {
        return false;
    }
    glfwMakeContextCurrent(app.window);
    std::string missing;
    if (!loadGlFunctions(reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress), &missing)) {
        showError("Драйвер не надає функцій OpenGL 3.3: " + missing);
        return false;
    }
    return true;
}

int runShot(const Options& o) {
    g_showDialogs = false;
    App app;
    if (!createContext(app, o, false)) {
        showError("Не вдалося створити контекст OpenGL 3.3");
        return 1;
    }
    std::string error;
    app.applyOptions(o);
    if (!app.renderer.init(app.system, &error, o.shadowSize)) {
        showError("Помилка ініціалізації:\n" + error);
        return 1;
    }

    const ViewPreset* preset = o.preset.empty() ? findPreset("overview") : findPreset(o.preset);
    if (preset == nullptr) {
        showError("Невідомий ракурс: " + o.preset);
        return 1;
    }
    app.days = o.hasTime ? o.days : preset->days;
    app.system.update(app.days);
    applyPreset(*preset, app.system, app.camera);
    if (o.hasCamera) {
        app.camera.lookAtPoint(Vec3(o.cameraEye[0], o.cameraEye[1], o.cameraEye[2]),
                               Vec3(o.cameraTarget[0], o.cameraTarget[1], o.cameraTarget[2]));
    }
    if (o.fovDegrees > 0.0f) {
        app.camera.fovY = radians(o.fovDegrees);
    }
    if (o.rollDegrees != 0.0f) {
        app.camera.rotateLocal(0.0f, 0.0f, radians(o.rollDegrees));
    }
    const std::string selectId = o.select.empty() ? preset->selectedId : (o.select == "none" ? "" : o.select);
    app.selectTarget(selectId.empty() ? -1 : app.indexOfId(selectId));
    app.settings.hud.daysPerSecond = app.daysPerSecond;

    Offscreen target;
    if (!target.create(o.width, o.height, o.samples)) {
        showError("Не вдалося створити позаекранний буфер кадру");
        return 1;
    }
    RenderTarget rt;
    rt.framebuffer = target.framebuffer();
    rt.width = o.width;
    rt.height = o.height;
    app.renderer.render(app.system, app.camera, app.settings, rt);
    glFinish();
    const GLenum err = glGetError();
    const Image image = target.read();
    if (!saveTga(std::filesystem::u8path(o.shotFile), image)) {
        showError("Не вдалося записати файл " + o.shotFile);
        return 1;
    }
    std::printf("%s: %dx%d, ракурс %s, день %.2f, помилка GL = 0x%X\n", o.shotFile.c_str(), o.width, o.height,
                preset->name.c_str(), app.days, err);
    app.renderer.shutdown();
    glfwDestroyWindow(app.window);
    return err == GL_NO_ERROR ? 0 : 2;
}

int runInteractive(const Options& o) {
    App app;
    if (!createContext(app, o, true)) {
        showError("Не вдалося створити вікно OpenGL 3.3. Потрібна відеокарта з підтримкою OpenGL 3.3.");
        return 1;
    }
    std::string error;
    app.applyOptions(o);
    if (!app.renderer.init(app.system, &error, o.shadowSize)) {
        showError("Помилка ініціалізації:\n" + error);
        return 1;
    }
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetKeyCallback(app.window, keyCallback);
    glfwSetMouseButtonCallback(app.window, mouseButtonCallback);
    glfwSetCursorPosCallback(app.window, cursorCallback);
    glfwSetScrollCallback(app.window, scrollCallback);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(app.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    const ViewPreset* start = o.preset.empty() ? findPreset("overview") : findPreset(o.preset);
    if (start == nullptr) {
        start = findPreset("overview");
    }
    app.days = o.hasTime ? o.days : start->days;
    app.system.update(app.days);
    applyPreset(*start, app.system, app.camera);
    if (!start->selectedId.empty() && !o.preset.empty()) {
        app.selectTarget(app.indexOfId(start->selectedId));
    }

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    double titleTimer = 0.0;
    int frames = 0;
    double fps = 0.0;
    while (!glfwWindowShouldClose(app.window)) {
        glfwPollEvents();
        const auto now = Clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - last).count(), 0.1f);
        last = now;

        if (!app.paused) {
            app.days += dt * app.daysPerSecond;
        }
        app.system.update(app.days);
        app.updateCamera(dt);

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(app.window, &fbw, &fbh);
        if (fbw > 0 && fbh > 0) {
            app.settings.hud.paused = app.paused;
            app.settings.hud.orbitMode = app.orbitMode;
            app.settings.hud.daysPerSecond = app.daysPerSecond;
            RenderTarget rt;
            rt.framebuffer = 0;
            rt.width = fbw;
            rt.height = fbh;
            app.renderer.render(app.system, app.camera, app.settings, rt);

            if (app.wantScreenshot) {
                app.wantScreenshot = false;
                Image image;
                image.width = fbw;
                image.height = fbh;
                image.channels = 3;
                std::vector<unsigned char> raw(static_cast<size_t>(fbw) * fbh * 3);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, raw.data());
                image.pixels.resize(raw.size());
                for (int y = 0; y < fbh; ++y) {
                    std::copy(&raw[static_cast<size_t>(fbh - 1 - y) * fbw * 3], &raw[static_cast<size_t>(fbh - y) * fbw * 3],
                              &image.pixels[static_cast<size_t>(y) * fbw * 3]);
                }
                char name[64];
                std::snprintf(name, sizeof(name), "screenshot_%03d.tga", ++app.screenshotCounter);
                saveTga(name, image);
            }
        }
        glfwSwapBuffers(app.window);

        ++frames;
        titleTimer += dt;
        if (titleTimer >= 0.4) {
            fps = frames / titleTimer;
            frames = 0;
            titleTimer = 0.0;
            glfwSetWindowTitle(app.window, app.title(fps).c_str());
        }
    }
    app.renderer.shutdown();
    glfwDestroyWindow(app.window);
    return 0;
}

}  // namespace

// Аргументи командного рядка в UTF-8: шляхи користувача містять кирилицю, а argv у Windows —
// в ANSI-кодуванні, яке std::filesystem не завжди вміє перетворити
std::vector<std::string> commandLineUtf8(int argc, char** argv) {
    std::vector<std::string> args;
#ifdef _WIN32
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide != nullptr) {
        for (int i = 0; i < count; ++i) {
            const int n = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
            if (n > 1) {
                WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, &s[0], n, nullptr, nullptr);
            }
            args.push_back(s);
        }
        LocalFree(wide);
        return args;
    }
#endif
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return args;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // Програма збирається без консолі; якщо її запущено з термінала — виводимо текст у нього
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
#endif
    Options options;
    if (!parseArguments(commandLineUtf8(argc, argv), options)) {
        std::fputs(kUsage, stderr);
        return 1;
    }
    if (options.help) {
        std::fputs(kUsage, stdout);
        return 0;
    }
    if (options.listPresets) {
        for (const ViewPreset& p : viewPresets()) {
            std::printf("%s\n", p.name.c_str());
        }
        return 0;
    }
    if (!options.dumpBodies.empty()) {
        // Таблиця тіл сцени (для звіту): береться з тих самих даних, що малює програма
        const SolarSystem system;
        std::string text = "id\tname\tparent\tradius\torbit\torbit_period\trotation_period\ttilt\tphase\n";
        char line[256];
        for (int i = 0; i < system.bodyCount(); ++i) {
            const BodyDef& d = system.def(i);
            std::snprintf(line, sizeof(line), "%s\t%s\t%d\t%g\t%g\t%g\t%g\t%g\t%g\n", d.id.c_str(), d.name.c_str(),
                          d.parent, d.radius, d.orbitRadius, d.orbitPeriod, d.rotationPeriod, d.axialTilt, d.phase0);
            text += line;
        }
        const std::filesystem::path out = std::filesystem::u8path(options.dumpBodies);
        return assets::writeFile(out, reinterpret_cast<const unsigned char*>(text.data()), text.size()) ? 0 : 1;
    }
    if (!assets::locate()) {
        showError("Не знайдено каталог assets (шейдери, текстури, моделі). Він має лежати поруч із bin/.");
        return 1;
    }
    if (!glfwInit()) {
        showError("Не вдалося ініціалізувати GLFW");
        return 1;
    }
    const int code = options.shotFile.empty() ? runInteractive(options) : runShot(options);
    glfwTerminate();
    return code;
}
