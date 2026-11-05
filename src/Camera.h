#pragma once

// Повністю тривимірна камера.
//
// Стан — положення і кватерніон орієнтації (локальні осі: x праворуч, y вгору, −z уперед).
// Повороти навколо локальних осей (рискання, тангаж, крен) накопичуються множенням
// кватерніонів, тому немає «кардану» й обмежень на кут тангажа: камера може зробити повний
// оберт в усіх площинах і пройти через полюс. Матриця виду — обернена до матриці камери.
//
// Режим орбіти (орбітальне слідкування) лише задає положення й орієнтацію за азимутом,
// висотою й відстанню до цілі; далі вільний політ продовжується з того самого стану.

#include "Math.h"

class Camera {
public:
    Camera();

    gfx::Vec3 position{0.0f, 40.0f, 140.0f};
    gfx::Quat orientation;
    float fovY = gfx::radians(55.0f);
    float zNear = 0.1f;
    float zFar = 2000.0f;

    gfx::Vec3 forward() const { return gfx::rotateVector(orientation, gfx::Vec3(0.0f, 0.0f, -1.0f)); }
    gfx::Vec3 right() const { return gfx::rotateVector(orientation, gfx::Vec3(1.0f, 0.0f, 0.0f)); }
    gfx::Vec3 up() const { return gfx::rotateVector(orientation, gfx::Vec3(0.0f, 1.0f, 0.0f)); }

    // Поворот навколо локальних осей, радіани: yaw — навколо «вгору» (додатний — ліворуч),
    // pitch — навколо «праворуч» (додатний — угору), roll — навколо «уперед» (додатний — за годинниковою)
    void rotateLocal(float yaw, float pitch, float roll);
    // Зсув у системі камери: x — праворуч, y — вгору, z — уперед
    void moveLocal(const gfx::Vec3& delta);
    // Ставить око в eye і повертає камеру до target (орієнтація «вгору» — якомога ближче до up)
    void lookAtPoint(const gfx::Vec3& eye, const gfx::Vec3& target,
                     const gfx::Vec3& up = gfx::Vec3(0.0f, 1.0f, 0.0f));
    // Орбітальне положення навколо target: азимут (навколо y), висота над площиною xOz, відстань
    void orbit(const gfx::Vec3& target, float distance, float azimuth, float elevation);

    gfx::Mat4 view() const;
    gfx::Mat4 projection(float aspect) const { return gfx::perspective(fovY, aspect, zNear, zFar); }
};
