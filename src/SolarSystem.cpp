#include "SolarSystem.h"

#include <cmath>
#include <cstdint>

using namespace gfx;

namespace {

// Детермінований генератор (xorshift32): однакові астероїди на всіх комп'ютерах і в тестах
struct Rng {
    std::uint32_t s = 2463534242u;
    float next() {   // [0; 1)
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s & 0xFFFFFF) / 16777216.0f;
    }
    float range(float a, float b) { return a + (b - a) * next(); }
};

BodyDef body(const char* id, const char* name, int parent, float radius, float orbitRadius, float orbitPeriod,
             float rotationPeriod, float tilt, float phase0) {
    BodyDef d;
    d.id = id;
    d.name = name;
    d.parent = parent;
    d.radius = radius;
    d.orbitRadius = orbitRadius;
    d.orbitPeriod = orbitPeriod;
    d.rotationPeriod = rotationPeriod;
    d.axialTilt = tilt;
    d.phase0 = phase0;
    return d;
}

}  // namespace

SolarSystem::SolarSystem() {
    // Порядок важливий: батько стоїть у списку раніше за дітей
    BodyDef sun = body("sun", "Сонце", -1, 7.0f, 0.0f, 0.0f, 25.4f, 7.25f, 0.0f);
    sun.star = true;
    defs_.push_back(sun);                                                              // 0

    defs_.push_back(body("mercury", "Меркурій", 0, 0.45f, 14.0f, 87.97f, 58.65f, 0.03f, 40.0f));   // 1
    BodyDef& mercury = defs_.back();
    mercury.normalMap = true;
    mercury.specularStrength = 0.05f;

    BodyDef venus = body("venus", "Венера", 0, 1.15f, 21.0f, 224.7f, 243.0f, 177.4f, 300.0f);
    venus.atmosphere = {1.0f, 0.85f, 0.55f};
    venus.atmosphereStrength = 0.55f;
    defs_.push_back(venus);                                                            // 2

    BodyDef earth = body("earth", "Земля", 0, 1.3f, 30.0f, 365.25f, 1.0f, 23.44f, 200.0f);
    earth.specularMap = true;
    earth.shininess = 70.0f;
    earth.specularStrength = 0.9f;
    earth.atmosphere = {0.35f, 0.6f, 1.0f};
    earth.atmosphereStrength = 0.9f;
    defs_.push_back(earth);                                                            // 3

    BodyDef moon = body("moon", "Місяць", 3, 0.36f, 3.4f, 27.32f, 27.32f, 1.54f, 200.0f);
    moon.normalMap = true;
    moon.specularStrength = 0.04f;
    defs_.push_back(moon);                                                             // 4

    BodyDef mars = body("mars", "Марс", 0, 0.7f, 40.0f, 687.0f, 1.026f, 25.19f, 120.0f);
    mars.normalMap = true;
    mars.atmosphere = {1.0f, 0.55f, 0.35f};
    mars.atmosphereStrength = 0.25f;
    defs_.push_back(mars);                                                             // 5

    BodyDef jupiter = body("jupiter", "Юпітер", 0, 4.2f, 78.0f, 4333.0f, 0.414f, 3.13f, 20.0f);
    jupiter.shininess = 16.0f;
    defs_.push_back(jupiter);                                                          // 6

    defs_.push_back(body("io", "Іо", 6, 0.38f, 6.4f, 1.769f, 1.769f, 0.0f, 20.0f));              // 7
    defs_.push_back(body("europa", "Європа", 6, 0.33f, 7.9f, 3.551f, 3.551f, 0.1f, 110.0f));     // 8
    defs_.push_back(body("ganymede", "Ганімед", 6, 0.55f, 9.7f, 7.155f, 7.155f, 0.2f, 200.0f));  // 9
    defs_.push_back(body("callisto", "Каллісто", 6, 0.5f, 11.9f, 16.69f, 16.69f, 0.2f, 290.0f)); // 10

    BodyDef saturn = body("saturn", "Сатурн", 0, 3.6f, 108.0f, 10759.0f, 0.444f, 26.73f, 250.0f);
    saturn.ring = true;
    saturn.ringInner = 1.28f;
    saturn.ringOuter = 2.35f;
    saturn.shininess = 16.0f;
    defs_.push_back(saturn);                                                           // 11

    BodyDef titan = body("titan", "Титан", 11, 0.5f, 12.4f, 15.95f, 15.95f, 0.3f, 60.0f);
    titan.atmosphere = {1.0f, 0.65f, 0.25f};
    titan.atmosphereStrength = 0.6f;
    defs_.push_back(titan);                                                            // 12

    BodyDef uranus = body("uranus", "Уран", 0, 2.0f, 134.0f, 30687.0f, 0.718f, 97.77f, 80.0f);
    uranus.atmosphere = {0.55f, 0.9f, 0.95f};
    uranus.atmosphereStrength = 0.45f;
    defs_.push_back(uranus);                                                           // 13

    BodyDef neptune = body("neptune", "Нептун", 0, 1.9f, 156.0f, 60190.0f, 0.671f, 28.32f, 330.0f);
    neptune.atmosphere = {0.3f, 0.5f, 1.0f};
    neptune.atmosphereStrength = 0.6f;
    defs_.push_back(neptune);                                                          // 14

    states_.resize(defs_.size());

    // Пояс астероїдів між Марсом і Юпітером
    Rng rng;
    const int count = 2600;
    asteroids_.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float r = rng.range(kBeltInner, kBeltOuter);
        const float a = rng.range(0.0f, 2.0f * kPi);
        const float y = (rng.next() + rng.next() - 1.0f) * 1.6f;
        const Vec3 axis = normalize(Vec3(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)) + Vec3(0.01f, 0.02f, 0.03f));
        const float size = rng.range(0.05f, 0.2f) * (rng.next() < 0.06f ? 2.2f : 1.0f);
        asteroids_.push_back(translate(Vec3(r * std::cos(a), y, r * std::sin(a))) * rotate(axis, rng.range(0.0f, 6.28f)) *
                             scale(Vec3(size, size * rng.range(0.7f, 1.0f), size * rng.range(0.8f, 1.0f))));
    }

    update(0.0);
}

void SolarSystem::update(double days) {
    time_ = days;
    for (size_t i = 0; i < defs_.size(); ++i) {
        const BodyDef& d = defs_[i];
        BodyState& s = states_[i];
        const Vec3 parentPos = d.parent >= 0 ? states_[static_cast<size_t>(d.parent)].position : Vec3();

        // Положення на орбіті
        const double orbitAngle =
            radians(d.phase0) + (d.orbitPeriod > 0.0f ? 2.0 * kPi * days / d.orbitPeriod : 0.0);
        const Mat4 frame = translate(parentPos) * rotateY(static_cast<float>(std::fmod(orbitAngle, 2.0 * kPi))) *
                           translate(Vec3(d.orbitRadius, 0.0f, 0.0f));
        s.position = transformPoint(frame, Vec3());
        s.orbitMatrix = translate(parentPos) * scale(d.orbitRadius);

        // Власне обертання: нахил осі стоїть ліворуч від повороту, тому не залежить від кута орбіти
        // Супутники з однаковими періодами обігу й обертання (приливний захват) завжди повернуті до
        // планети одним боком: кут обертання збігається з кутом орбіти (+180°)
        const bool locked = d.parent >= 0 && d.rotationPeriod == d.orbitPeriod;
        const double spin = locked ? orbitAngle + kPi : 2.0 * kPi * days * kSpinScale / d.rotationPeriod;
        s.model = translate(s.position) * rotateZ(radians(d.axialTilt)) *
                  rotateY(static_cast<float>(std::fmod(spin, 2.0 * kPi))) * scale(d.radius);
    }

    // Корабель летить колом навколо Сонця в площині, нахиленій на 8° до екліптики
    const float shipRadius = 44.0f;
    const float shipTilt = radians(8.0f);
    const float shipAngle = static_cast<float>(std::fmod(radians(75.0f) + 2.0 * kPi * days / 520.0, 2.0 * kPi));
    const Mat4 plane = rotateX(shipTilt) * rotateY(shipAngle);
    ship_.position = transformPoint(plane, Vec3(shipRadius, 0.0f, 0.0f));
    ship_.forward = normalize(transformDirection(plane, Vec3(0.0f, 0.0f, -1.0f)));
    const Vec3 shipUp = transformDirection(rotateX(shipTilt), Vec3(0.0f, 1.0f, 0.0f));
    ship_.model = translate(ship_.position) * lookRotation(ship_.forward, shipUp) * scale(kShipScale);

    beltRotation_ = rotateY(static_cast<float>(std::fmod(2.0 * kPi * days / 1500.0, 2.0 * kPi)));
}

int SolarSystem::indexOf(const std::string& id) const {
    for (size_t i = 0; i < defs_.size(); ++i) {
        if (defs_[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Vec3 SolarSystem::targetPosition(int selectable) const {
    return selectable == shipIndex() ? ship_.position : states_[static_cast<size_t>(selectable)].position;
}

float SolarSystem::targetRadius(int selectable) const {
    return selectable == shipIndex() ? kShipScale * 1.6f : defs_[static_cast<size_t>(selectable)].radius;
}

std::string SolarSystem::targetName(int selectable) const {
    return selectable == shipIndex() ? "Корабель" : defs_[static_cast<size_t>(selectable)].name;
}

Mat4 SolarSystem::ringModel(int body) const {
    const BodyDef& d = defs_[static_cast<size_t>(body)];
    return translate(states_[static_cast<size_t>(body)].position) * rotateZ(radians(d.axialTilt)) * scale(d.radius);
}
