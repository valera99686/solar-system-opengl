#include "Camera.h"

using namespace gfx;

Camera::Camera() { lookAtPoint(position, Vec3(0.0f, 0.0f, 0.0f)); }

void Camera::rotateLocal(float yaw, float pitch, float roll) {
    // Права множення = поворот у локальній системі: спершу крен, потім тангаж, потім рискання
    const Quat q = quatFromAxisAngle(Vec3(0, 1, 0), yaw) * quatFromAxisAngle(Vec3(1, 0, 0), pitch) *
                   quatFromAxisAngle(Vec3(0, 0, -1), roll);
    orientation = normalize(orientation * q);
}

void Camera::moveLocal(const Vec3& d) {
    position += right() * d.x + up() * d.y + forward() * d.z;
}

void Camera::lookAtPoint(const Vec3& eye, const Vec3& target, const Vec3& worldUp) {
    position = eye;
    const Vec3 f = normalize(target - eye);
    Vec3 r = cross(f, worldUp);
    if (length(r) < 1e-6f) {   // дивимося вздовж worldUp — беремо довільну перпендикулярну вісь
        r = cross(f, Vec3(0.0f, 0.0f, 1.0f));
        if (length(r) < 1e-6f) {
            r = cross(f, Vec3(1.0f, 0.0f, 0.0f));
        }
    }
    r = normalize(r);
    const Vec3 u = cross(r, f);
    orientation = quatFromBasis(r, u, -f);
}

void Camera::orbit(const Vec3& target, float distance, float azimuth, float elevation) {
    const float maxElevation = radians(89.0f);
    elevation = clampf(elevation, -maxElevation, maxElevation);
    const Vec3 offset(std::cos(elevation) * std::cos(azimuth), std::sin(elevation),
                      std::cos(elevation) * std::sin(azimuth));
    lookAtPoint(target + offset * distance, target);
}

Mat4 Camera::view() const {
    // View = (T(p) · R)^(−1) = Rᵀ · T(−p)
    return transpose(toMat4(orientation)) * translate(-position);
}
