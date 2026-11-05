#pragma once

// Власна лінійна алгебра для графіки: вектори, матриці 4×4 і кватерніони.
// Усі перетворення сцени (модель, вид, проєкція, нормалі, тіні) будуються з цих
// типів; бібліотеки GLM чи подібні не використовуються.
//
// Угоди — ті самі, що в OpenGL:
//   * Mat4 зберігається по стовпцях: елемент (рядок r, стовпець c) = m[c * 4 + r];
//   * вектор-стовпець множиться зліва: v' = M · v, тому добуток A * B означає
//     «спочатку B, потім A»;
//   * система координат правостороння, камера дивиться вздовж −z;
//   * проєкція відображає видимий об'єм у куб [−1; 1]³ (NDC OpenGL).
//
// Імена zNear/zFar замість near/far — у windows.h це порожні макроси.

#include <cmath>
#include <cstddef>

namespace gfx {

constexpr float kPi = 3.14159265358979323846f;

inline float radians(float degrees) { return degrees * kPi / 180.0f; }
inline float degrees(float radians) { return radians * 180.0f / kPi; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Вектори
// ---------------------------------------------------------------------------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(const Vec3& a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(const Vec3& a, float k) { return {a.x * k, a.y * k, a.z * k}; }
inline Vec3 operator*(float k, const Vec3& a) { return a * k; }
inline Vec3& operator+=(Vec3& a, const Vec3& b) { a = a + b; return a; }
inline Vec3& operator-=(Vec3& a, const Vec3& b) { a = a - b; return a; }

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3& a) {
    const float len = length(a);
    return len > 0.0f ? a * (1.0f / len) : Vec3(0.0f, 0.0f, 0.0f);
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------------------------
// Матриця 3×3 (нормалі) і 4×4
// ---------------------------------------------------------------------------
struct Mat3 {
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};   // по стовпцях
};

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};   // по стовпцях

    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.at(row, k) * b.at(k, c);
            }
            r.at(row, c) = sum;
        }
    }
    return r;
}

inline Vec4 operator*(const Mat4& a, const Vec4& v) {
    return {a.at(0, 0) * v.x + a.at(0, 1) * v.y + a.at(0, 2) * v.z + a.at(0, 3) * v.w,
            a.at(1, 0) * v.x + a.at(1, 1) * v.y + a.at(1, 2) * v.z + a.at(1, 3) * v.w,
            a.at(2, 0) * v.x + a.at(2, 1) * v.y + a.at(2, 2) * v.z + a.at(2, 3) * v.w,
            a.at(3, 0) * v.x + a.at(3, 1) * v.y + a.at(3, 2) * v.z + a.at(3, 3) * v.w};
}

// Точка (w = 1) і напрямок (w = 0) без перспективного ділення
inline Vec3 transformPoint(const Mat4& a, const Vec3& p) {
    const Vec4 r = a * Vec4(p, 1.0f);
    return {r.x, r.y, r.z};
}
inline Vec3 transformDirection(const Mat4& a, const Vec3& d) {
    const Vec4 r = a * Vec4(d, 0.0f);
    return {r.x, r.y, r.z};
}

inline Mat4 translate(const Vec3& t) {
    Mat4 r;
    r.at(0, 3) = t.x;
    r.at(1, 3) = t.y;
    r.at(2, 3) = t.z;
    return r;
}

inline Mat4 scale(const Vec3& s) {
    Mat4 r;
    r.at(0, 0) = s.x;
    r.at(1, 1) = s.y;
    r.at(2, 2) = s.z;
    return r;
}
inline Mat4 scale(float s) { return scale(Vec3(s, s, s)); }

// Поворот на кут angle (радіани) навколо довільної осі за формулою Родрига
inline Mat4 rotate(const Vec3& axis, float angle) {
    const Vec3 a = normalize(axis);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0f - c;
    Mat4 r;
    r.at(0, 0) = t * a.x * a.x + c;
    r.at(0, 1) = t * a.x * a.y - s * a.z;
    r.at(0, 2) = t * a.x * a.z + s * a.y;
    r.at(1, 0) = t * a.x * a.y + s * a.z;
    r.at(1, 1) = t * a.y * a.y + c;
    r.at(1, 2) = t * a.y * a.z - s * a.x;
    r.at(2, 0) = t * a.x * a.z - s * a.y;
    r.at(2, 1) = t * a.y * a.z + s * a.x;
    r.at(2, 2) = t * a.z * a.z + c;
    return r;
}
inline Mat4 rotateX(float angle) { return rotate(Vec3(1, 0, 0), angle); }
inline Mat4 rotateY(float angle) { return rotate(Vec3(0, 1, 0), angle); }
inline Mat4 rotateZ(float angle) { return rotate(Vec3(0, 0, 1), angle); }

inline Mat4 transpose(const Mat4& a) {
    Mat4 r;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.at(row, col) = a.at(col, row);
        }
    }
    return r;
}

// Обернена матриця методом Гаусса–Жордана з вибором головного елемента.
// Якщо матриця виродженa, повертає одиничну й записує false в *ok.
inline Mat4 inverse(const Mat4& a, bool* ok = nullptr) {
    double w[4][8];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            w[r][c] = a.at(r, c);
            w[r][c + 4] = (r == c) ? 1.0 : 0.0;
        }
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int r = col + 1; r < 4; ++r) {
            if (std::fabs(w[r][col]) > std::fabs(w[pivot][col])) {
                pivot = r;
            }
        }
        if (std::fabs(w[pivot][col]) < 1e-12) {
            if (ok) {
                *ok = false;
            }
            return Mat4();
        }
        if (pivot != col) {
            for (int c = 0; c < 8; ++c) {
                const double tmp = w[col][c];
                w[col][c] = w[pivot][c];
                w[pivot][c] = tmp;
            }
        }
        const double inv = 1.0 / w[col][col];
        for (int c = 0; c < 8; ++c) {
            w[col][c] *= inv;
        }
        for (int r = 0; r < 4; ++r) {
            if (r == col) {
                continue;
            }
            const double f = w[r][col];
            for (int c = 0; c < 8; ++c) {
                w[r][c] -= f * w[col][c];
            }
        }
    }
    if (ok) {
        *ok = true;
    }
    Mat4 r;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.at(row, col) = static_cast<float>(w[row][col + 4]);
        }
    }
    return r;
}

// Матриця нормалей: (M₃ₓ₃)^(−T). Зберігає перпендикулярність нормалей до поверхні
// за довільного (також нерівномірного) масштабу.
inline Mat3 normalMatrix(const Mat4& model) {
    const Mat4 invT = transpose(inverse(model));
    Mat3 r;
    for (int c = 0; c < 3; ++c) {
        for (int row = 0; row < 3; ++row) {
            r.m[c * 3 + row] = invT.at(row, c);
        }
    }
    return r;
}

// Вид: система спостерігача з оком eye, що дивиться на center; up — орієнтовний «вгору».
// Рядки матриці — базис s (вправо), u (вгору), −f (назад); остання колонка переносить
// око в початок координат.
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r;
    r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;  r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;  r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = dot(f, eye);
    return r;
}

// Матриця повороту, що спрямовує локальну вісь −z вздовж forward, а локальну +y — якомога
// ближче до up (орієнтація літака, камери-«білборда»). Стовпці: праворуч, вгору, назад.
inline Mat4 lookRotation(const Vec3& forward, const Vec3& up) {
    const Vec3 back = normalize(-forward);
    const Vec3 right = normalize(cross(up, back));
    const Vec3 u = cross(back, right);
    Mat4 r;
    r.at(0, 0) = right.x; r.at(1, 0) = right.y; r.at(2, 0) = right.z;
    r.at(0, 1) = u.x;     r.at(1, 1) = u.y;     r.at(2, 1) = u.z;
    r.at(0, 2) = back.x;  r.at(1, 2) = back.y;  r.at(2, 2) = back.z;
    return r;
}

// Перспективна проєкція (еквівалент gluPerspective): fovY — вертикальний кут огляду.
inline Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
    const float t = 1.0f / std::tan(fovY * 0.5f);
    Mat4 r;
    r.at(0, 0) = t / aspect;
    r.at(1, 1) = t;
    r.at(2, 2) = -(zFar + zNear) / (zFar - zNear);
    r.at(2, 3) = -2.0f * zFar * zNear / (zFar - zNear);
    r.at(3, 2) = -1.0f;
    r.at(3, 3) = 0.0f;
    return r;
}

// Ортогональна проєкція (еквівалент glOrtho)
inline Mat4 ortho(float l, float r_, float b, float t, float zNear, float zFar) {
    Mat4 r;
    r.at(0, 0) = 2.0f / (r_ - l);
    r.at(1, 1) = 2.0f / (t - b);
    r.at(2, 2) = -2.0f / (zFar - zNear);
    r.at(0, 3) = -(r_ + l) / (r_ - l);
    r.at(1, 3) = -(t + b) / (t - b);
    r.at(2, 3) = -(zFar + zNear) / (zFar - zNear);
    return r;
}

// ---------------------------------------------------------------------------
// Кватерніони: орієнтація вільної камери без «кардану» (gimbal lock)
// ---------------------------------------------------------------------------
struct Quat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Quat operator*(const Quat& a, const Quat& b) {   // добуток Гамільтона: спершу b, потім a
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

inline Quat normalize(const Quat& q) {
    const float len = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (len <= 0.0f) {
        return Quat();
    }
    const float k = 1.0f / len;
    return {q.w * k, q.x * k, q.y * k, q.z * k};
}

inline Quat quatFromAxisAngle(const Vec3& axis, float angle) {
    const Vec3 a = normalize(axis);
    const float h = angle * 0.5f;
    const float s = std::sin(h);
    return {std::cos(h), a.x * s, a.y * s, a.z * s};
}

inline Vec3 rotateVector(const Quat& q, const Vec3& v) {
    const Vec3 u(q.x, q.y, q.z);
    const Vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

inline Mat4 toMat4(const Quat& q) {
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    Mat4 r;
    r.at(0, 0) = 1.0f - 2.0f * (yy + zz);
    r.at(0, 1) = 2.0f * (xy - wz);
    r.at(0, 2) = 2.0f * (xz + wy);
    r.at(1, 0) = 2.0f * (xy + wz);
    r.at(1, 1) = 1.0f - 2.0f * (xx + zz);
    r.at(1, 2) = 2.0f * (yz - wx);
    r.at(2, 0) = 2.0f * (xz - wy);
    r.at(2, 1) = 2.0f * (yz + wx);
    r.at(2, 2) = 1.0f - 2.0f * (xx + yy);
    return r;
}

// Кватерніон за ортонормованим базисом (стовпці матриці повороту: right, up, back)
inline Quat quatFromBasis(const Vec3& right, const Vec3& up, const Vec3& back) {
    const float m00 = right.x, m10 = right.y, m20 = right.z;
    const float m01 = up.x, m11 = up.y, m21 = up.z;
    const float m02 = back.x, m12 = back.y, m22 = back.z;
    const float trace = m00 + m11 + m22;
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    } else {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return normalize(q);
}

}  // namespace gfx
