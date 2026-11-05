#pragma once

// Модель сцени: Сонце, вісім планет, шість супутників, космічний корабель і пояс астероїдів.
//
// Усе положення тіл — результат множення матриць трансформацій за ієрархією «батько → дитина»:
//
//   frame   = T(позиція батька) · Ry(кут орбіти) · T(радіус орбіти, 0, 0)    — положення на орбіті
//   model   = T(позиція) · Rz(нахил осі) · Ry(кут обертання) · S(радіус)       — матриця моделі
//
// Супутник бере позицію батьківського тіла, тож рухається разом із планетою. Нахил осі
// не залежить від кута орбіти, завдяки чому виникають пори року. Розміри й відстані
// стиснуті (нереалістичні), а співвідношення періодів обертання й обігу — справжні.

#include <string>
#include <vector>

#include "Math.h"

struct BodyDef {
    std::string id;          // ідентифікатор і ім'я файлів текстур
    std::string name;        // українська назва
    int parent = -1;         // індекс батьківського тіла (орбіта навколо нього) або −1
    float radius = 1.0f;
    float orbitRadius = 0.0f;
    float orbitPeriod = 0.0f;      // доби; 0 — нерухоме
    float rotationPeriod = 1.0f;   // доби
    float axialTilt = 0.0f;        // градуси
    float phase0 = 0.0f;           // початковий кут на орбіті, градуси
    bool star = false;
    bool specularMap = false;      // є файл <id>_s.tga
    bool normalMap = false;        // є файл <id>_n.tga
    float shininess = 24.0f;
    float specularStrength = 0.08f;
    gfx::Vec3 atmosphere{0.0f, 0.0f, 0.0f};   // колір ореолу
    float atmosphereStrength = 0.0f;
    bool ring = false;
    float ringInner = 0.0f;        // у радіусах планети
    float ringOuter = 0.0f;
};

struct BodyState {
    gfx::Vec3 position;
    gfx::Mat4 model;           // T · Rz · Ry · S
    gfx::Mat4 orbitMatrix;     // T(позиція батька) · S(радіус орбіти): коло одиничного радіуса → орбіта
};

struct ShipState {
    gfx::Vec3 position;
    gfx::Vec3 forward;
    gfx::Mat4 model;
};

class SolarSystem {
public:
    static constexpr float kSpinScale = 0.1f;   // осьове обертання сповільнене в 10 разів для зручності огляду

    SolarSystem();

    // Перераховує матриці всіх тіл для моменту часу (доби від початку відліку)
    void update(double days);
    double time() const { return time_; }

    int bodyCount() const { return static_cast<int>(defs_.size()); }
    const BodyDef& def(int i) const { return defs_[static_cast<size_t>(i)]; }
    const BodyState& state(int i) const { return states_[static_cast<size_t>(i)]; }
    int indexOf(const std::string& id) const;   // −1, якщо немає

    // Об'єкти, які можна вибрати: усі тіла й корабель (останній індекс)
    int selectableCount() const { return bodyCount() + 1; }
    int shipIndex() const { return bodyCount(); }
    gfx::Vec3 targetPosition(int selectable) const;
    float targetRadius(int selectable) const;
    std::string targetName(int selectable) const;

    // Кільце Сатурна: модель = T · Rz(нахил) · S(радіус) (без осьового обертання)
    gfx::Mat4 ringModel(int body) const;

    const ShipState& ship() const { return ship_; }
    static constexpr float kShipScale = 0.55f;

    // Пояс астероїдів: матриці екземплярів фіксовані, а увесь пояс повільно обертається
    const std::vector<gfx::Mat4>& asteroidMatrices() const { return asteroids_; }
    gfx::Mat4 beltRotation() const { return beltRotation_; }
    static constexpr float kBeltInner = 49.0f;
    static constexpr float kBeltOuter = 59.0f;

private:
    std::vector<BodyDef> defs_;
    std::vector<BodyState> states_;
    ShipState ship_;
    std::vector<gfx::Mat4> asteroids_;
    gfx::Mat4 beltRotation_;
    double time_ = 0.0;
};
