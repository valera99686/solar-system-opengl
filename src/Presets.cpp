#include "Presets.h"

#include <cmath>

using namespace gfx;

namespace {

// Напрям від тіла на Сонце (Сонце лежить у початку координат)
Vec3 towardSun(const Vec3& position) { return normalize(-position); }

// Камера навколо тіла з боку Сонця: напрям на Сонце повернуто на lateral градусів навколо осі y
// й піднято на elevation градусів, тож денний бік видно, а тінь і термінатор — теж
void viewLit(Camera& camera, const Vec3& body, float distance, float lateralDeg, float elevationDeg) {
    const Vec3 dir = transformDirection(rotateY(radians(lateralDeg)), towardSun(body));
    const float el = radians(elevationDeg);
    const Vec3 eye = body + (dir * std::cos(el) + Vec3(0.0f, 1.0f, 0.0f) * std::sin(el)) * distance;
    camera.lookAtPoint(eye, body);
}

}  // namespace

const std::vector<ViewPreset>& viewPresets() {
    static const std::vector<ViewPreset> presets = {
        {"overview", 0.0, ""},
        {"inner", 40.0, "earth"},
        {"earth", 100.0, "earth"},
        {"earth-close", 100.0, ""},
        {"moon", 100.0, ""},
        {"lunar-eclipse", 0.0, "moon"},
        {"solar-eclipse", 14.77, "moon"},
        {"mars", 300.0, "mars"},
        {"belt", 120.0, "ship"},
        {"jupiter", 900.0, "jupiter"},
        {"saturn", 3287.0, "saturn"},
        {"ship", 250.0, "ship"},
        {"sun", 0.0, "sun"},
    };
    return presets;
}

const ViewPreset* findPreset(const std::string& name) {
    for (const ViewPreset& p : viewPresets()) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

void applyPreset(const ViewPreset& preset, const SolarSystem& system, Camera& camera) {
    const Vec3 up(0.0f, 1.0f, 0.0f);
    const std::string& n = preset.name;
    if (n == "overview") {
        camera.lookAtPoint(Vec3(10.0f, 150.0f, 285.0f), Vec3(0.0f, -8.0f, 0.0f));
    } else if (n == "inner") {
        camera.lookAtPoint(Vec3(8.0f, 52.0f, 74.0f), Vec3(0.0f, 0.0f, 8.0f));
    } else if (n == "earth") {
        viewLit(camera, system.state(system.indexOf("earth")).position, 9.0f, 55.0f, 16.0f);
    } else if (n == "earth-close") {
        viewLit(camera, system.state(system.indexOf("earth")).position, 4.6f, 48.0f, 12.0f);
    } else if (n == "moon") {
        viewLit(camera, system.state(system.indexOf("moon")).position, 1.7f, 62.0f, 10.0f);
    } else if (n == "lunar-eclipse") {
        // Камера збоку від лінії Сонце–Земля–Місяць: Місяць позаду Землі, у її тіні
        const Vec3 earth = system.state(system.indexOf("earth")).position;
        const Vec3 moon = system.state(system.indexOf("moon")).position;
        const Vec3 side = normalize(cross(towardSun(earth), up));
        camera.lookAtPoint((earth + moon) * 0.5f + side * 8.5f + up * 2.0f, (earth + moon) * 0.5f);
    } else if (n == "solar-eclipse") {
        // Камера з боку Сонця, зсунута вбік: видно Місяць перед Землею й круглу тінь на її поверхні
        const Vec3 earth = system.state(system.indexOf("earth")).position;
        const Vec3 toSun = towardSun(earth);
        const Vec3 dir = transformDirection(rotateY(radians(38.0f)), toSun);
        camera.lookAtPoint(earth + dir * 7.0f + up * 1.2f, earth + toSun * 1.4f);
    } else if (n == "mars") {
        viewLit(camera, system.state(system.indexOf("mars")).position, 4.5f, 40.0f, 14.0f);
    } else if (n == "belt") {
        // Корабель летить уздовж внутрішнього краю поясу: камера позаду-збоку з боку астероїдів
        const Vec3 ship = system.ship().position;
        const Vec3 fwd = system.ship().forward;
        const Vec3 outward = normalize(Vec3(ship.x, 0.0f, ship.z));
        camera.lookAtPoint(ship - fwd * 5.5f + outward * 2.2f + up * 1.6f, ship + fwd * 3.0f + outward * 4.0f);
    } else if (n == "jupiter") {
        viewLit(camera, system.state(system.indexOf("jupiter")).position, 30.0f, 60.0f, 12.0f);
    } else if (n == "saturn") {
        viewLit(camera, system.state(system.indexOf("saturn")).position, 21.0f, 55.0f, 17.0f);
    } else if (n == "ship") {
        const Vec3 ship = system.ship().position;
        const Vec3 fwd = system.ship().forward;
        const Vec3 side = normalize(cross(fwd, up));
        camera.lookAtPoint(ship - fwd * 2.6f + side * 2.4f + up * 1.1f, ship);
    } else if (n == "sun") {
        camera.orbit(system.state(0).position, 34.0f, radians(35.0f), radians(10.0f));
    }
}
