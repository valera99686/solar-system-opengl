// Самоперевірка лабораторної роботи №3.
//
// Порівнює математику, ієрархію сцени, ресурси й відтворені кадри з очікуваннями, виведеними
// незалежно від коду програми (властивості матриць, геометрія затемнень, колір кубічної карти).
// Тести з OpenGL відтворюють кадри в позаекранний буфер у прихованому вікні.
// Запуск: bin/selftest.exe — друкує по рядку на перевірку й підсумок; код виходу 0 — усе гаразд.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "GlFuncs.h"

#include <GLFW/glfw3.h>

#include "Assets.h"
#include "Camera.h"
#include "Geometry.h"
#include "Hud.h"
#include "Mesh.h"
#include "Offscreen.h"
#include "Presets.h"
#include "Renderer.h"
#include "Shader.h"
#include "SolarSystem.h"
#include "Texture.h"

using namespace gfx;

namespace {

int g_passed = 0;
int g_failed = 0;

std::string num(double v, const char* fmt = "%.3g") {
    char buf[64];
    std::snprintf(buf, sizeof(buf), fmt, v);
    std::string s = buf;
    std::string out;
    for (char c : s) {
        if (c == '.') {
            out += ',';
        } else if (c == '-') {
            out += "−";
        } else {
            out += c;
        }
    }
    return out;
}

void check(const std::string& name, bool ok, const std::string& detail = "") {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
    }
    std::printf("[%s] %s%s%s\n", ok ? " OK " : "FAIL", name.c_str(), detail.empty() ? "" : " | ", detail.c_str());
}

double maxDiff(const Mat4& a, const Mat4& b) {
    double worst = 0.0;
    for (int i = 0; i < 16; ++i) {
        worst = std::max(worst, static_cast<double>(std::fabs(a.m[i] - b.m[i])));
    }
    return worst;
}

double dist(const Vec3& a, const Vec3& b) { return length(a - b); }

double angleBetween(const Vec3& a, const Vec3& b) {
    // atan2 стійкіший за acos поблизу 0° і 180°
    const Vec3 na = normalize(a), nb = normalize(b);
    return std::atan2(static_cast<double>(length(cross(na, nb))), static_cast<double>(dot(na, nb))) * 180.0 / kPi;
}

// ---------------------------------------------------------------------------
// Математика
// ---------------------------------------------------------------------------
void testMatrices() {
    // Порядок множення: A · B — спершу B. Поворот на 90° навколо z, потім зсув.
    const Mat4 tr = translate(Vec3(1, 2, 3)) * rotateZ(radians(90.0f));
    const Vec3 p = transformPoint(tr, Vec3(1, 0, 0));
    const Mat4 reversed = rotateZ(radians(90.0f)) * translate(Vec3(1, 2, 3));
    const Vec3 q = transformPoint(reversed, Vec3(1, 0, 0));
    check("добуток матриць: T · Rz(90°) · (1; 0; 0) = (1; 3; 3), обернений порядок дає інше",
          dist(p, Vec3(1, 3, 3)) < 1e-5 && dist(q, Vec3(-2, 2, 3)) < 1e-5,
          "(" + num(p.x) + "; " + num(p.y) + "; " + num(p.z) + ") проти (" + num(q.x) + "; " + num(q.y) + "; " +
              num(q.z) + ")");

    const Mat4 ry = rotateY(radians(90.0f));
    const Vec3 ry1 = transformPoint(ry, Vec3(1, 0, 0));
    const Vec3 axisY = transformPoint(rotate(Vec3(0, 2, 0), radians(33.0f)), Vec3(0, 5, 0));
    check("поворот навколо y на 90° переводить (1; 0; 0) у (0; 0; −1); вісь повороту нерухома",
          dist(ry1, Vec3(0, 0, -1)) < 1e-5 && dist(axisY, Vec3(0, 5, 0)) < 1e-5,
          "(" + num(ry1.x) + "; " + num(ry1.y) + "; " + num(ry1.z) + ")");

    const Mat4 m = translate(Vec3(3, -2, 5)) * rotate(Vec3(1, 2, 3), 0.7f) * scale(Vec3(2, 0.5f, 1.5f));
    bool ok = true;
    const Mat4 inv = inverse(m, &ok);
    const double err = maxDiff(m * inv, Mat4());
    Mat4 singular;
    singular.at(1, 1) = 0.0f;
    bool singularOk = true;
    inverse(singular, &singularOk);
    check("обернена матриця: M · M⁻¹ = E; вироджена матриця розпізнається", ok && err < 1e-5 && !singularOk,
          "похибка " + num(err));

    // Матриця нормалей зберігає перпендикулярність за нерівномірного масштабу
    const Mat4 nonUniform = rotate(Vec3(0, 1, 1), 0.9f) * scale(Vec3(3.0f, 0.4f, 1.0f));
    const Vec3 tangent = normalize(Vec3(1, 1, 0));
    const Vec3 normal = normalize(Vec3(1, -1, 0.5f) - tangent * dot(Vec3(1, -1, 0.5f), tangent));   // ⟂ tangent
    const Vec3 t2 = transformDirection(nonUniform, tangent);
    const Mat3 nm = normalMatrix(nonUniform);
    const Vec3 n2(nm.m[0] * normal.x + nm.m[3] * normal.y + nm.m[6] * normal.z,
                  nm.m[1] * normal.x + nm.m[4] * normal.y + nm.m[7] * normal.z,
                  nm.m[2] * normal.x + nm.m[5] * normal.y + nm.m[8] * normal.z);
    const Vec3 naive = transformDirection(nonUniform, normal);
    check("матриця нормалей: нормаль лишається перпендикулярною до дотичної, наївна M·n — ні",
          std::fabs(dot(t2, n2)) < 1e-4 && std::fabs(dot(t2, naive)) > 0.05,
          "n·t = " + num(dot(t2, n2)) + " проти " + num(dot(t2, naive)));

    // lookAt: властивості, що однозначно задають матрицю виду
    const Vec3 eye(3, 4, 5), target(-1, 0.5f, 2), up(0, 1, 0);
    const Mat4 v = lookAt(eye, target, up);
    const Vec3 e0 = transformPoint(v, eye);
    const Vec3 c0 = transformPoint(v, target);
    const Vec3 upView = transformDirection(v, up);
    const double det = v.at(0, 0) * (v.at(1, 1) * v.at(2, 2) - v.at(1, 2) * v.at(2, 1)) -
                       v.at(0, 1) * (v.at(1, 0) * v.at(2, 2) - v.at(1, 2) * v.at(2, 0)) +
                       v.at(0, 2) * (v.at(1, 0) * v.at(2, 1) - v.at(1, 1) * v.at(2, 0));
    check("lookAt: око → початок, ціль → вісь −z на відстані |c − e|, «вгору» → +y; det = +1",
          length(e0) < 1e-5 && std::fabs(c0.x) < 1e-5 && std::fabs(c0.y) < 1e-5 &&
              std::fabs(c0.z + length(target - eye)) < 1e-4 && std::fabs(upView.x) < 1e-5 && upView.y > 0.0f &&
              std::fabs(det - 1.0) < 1e-5,
          "ціль на z = " + num(c0.z) + ", det = " + num(det, "%.6f"));

    // perspective: площини відсікання, межі огляду, w = −z
    const float fov = radians(50.0f), aspect = 1.6f, zn = 0.5f, zf = 300.0f;
    const Mat4 P = perspective(fov, aspect, zn, zf);
    auto ndc = [&](const Vec3& pe) {
        const Vec4 c = P * Vec4(pe, 1.0f);
        return Vec4(c.x / c.w, c.y / c.w, c.z / c.w, c.w);
    };
    const float top = zn * std::tan(fov * 0.5f);
    const Vec4 a = ndc(Vec3(0, 0, -zn)), b = ndc(Vec3(0, 0, -zf)), c = ndc(Vec3(0, top, -zn)),
               d = ndc(Vec3(top * aspect, 0, -zn)), e = ndc(Vec3(0, 0, -10.0f));
    check("perspective: ближня площина → z = −1, дальня → +1, верхній край → y = 1, правий → x = 1, w = −z",
          std::fabs(a.z + 1) < 1e-5 && std::fabs(b.z - 1) < 1e-4 && std::fabs(c.y - 1) < 1e-5 &&
              std::fabs(d.x - 1) < 1e-5 && std::fabs(e.w - 10.0f) < 1e-5,
          "z(близька) = " + num(a.z) + ", z(дальня) = " + num(b.z, "%.5f"));

    const Mat4 O = ortho(-4, 6, -2, 8, 1, 21);
    const Vec3 o1 = transformPoint(O, Vec3(-4, -2, -1)), o2 = transformPoint(O, Vec3(6, 8, -21));
    check("ortho: кути паралелепіпеда → (−1; −1; −1) і (1; 1; 1)",
          dist(o1, Vec3(-1, -1, -1)) < 1e-5 && dist(o2, Vec3(1, 1, 1)) < 1e-5);

    // Кватерніони проти матриць
    const Vec3 axis = normalize(Vec3(0.3f, -0.8f, 0.5f));
    const Quat q1 = quatFromAxisAngle(axis, 1.1f), q2 = quatFromAxisAngle(Vec3(0, 1, 0), -0.6f);
    const double e1 = maxDiff(toMat4(q1), rotate(axis, 1.1f));
    const double e2 = maxDiff(toMat4(q2 * q1), toMat4(q2) * toMat4(q1));
    const Vec3 rv = rotateVector(q1, Vec3(1, 2, 3)), rm = transformDirection(rotate(axis, 1.1f), Vec3(1, 2, 3));
    const Mat4 rm4 = toMat4(q1);
    const Quat back = quatFromBasis(Vec3(rm4.at(0, 0), rm4.at(1, 0), rm4.at(2, 0)),
                                    Vec3(rm4.at(0, 1), rm4.at(1, 1), rm4.at(2, 1)),
                                    Vec3(rm4.at(0, 2), rm4.at(1, 2), rm4.at(2, 2)));
    const double sameQ = std::fabs(back.w * q1.w + back.x * q1.x + back.y * q1.y + back.z * q1.z);
    check("кватерніон = матриця повороту; добуток відповідає добутку матриць; базис → кватерніон",
          e1 < 1e-5 && e2 < 1e-5 && dist(rv, rm) < 1e-5 && std::fabs(sameQ - 1.0) < 1e-5,
          "похибки " + num(e1) + " і " + num(e2));

    const Mat4 lr = lookRotation(normalize(Vec3(1, 2, -1)), Vec3(0, 1, 0));
    const Vec3 f = transformDirection(lr, Vec3(0, 0, -1));
    const Vec3 rx = transformDirection(lr, Vec3(1, 0, 0)), uy = transformDirection(lr, Vec3(0, 1, 0));
    check("lookRotation: −z → напрямок погляду, осі ортогональні, права система",
          dist(f, normalize(Vec3(1, 2, -1))) < 1e-5 && std::fabs(dot(rx, uy)) < 1e-5 &&
              dist(cross(rx, uy), -f) < 1e-5);
}

// ---------------------------------------------------------------------------
// Камера
// ---------------------------------------------------------------------------
void testCamera() {
    Camera cam;
    cam.position = Vec3(5, -3, 8);
    cam.orientation = normalize(quatFromAxisAngle(Vec3(1, 2, 3), 0.8f));
    const Mat4 cameraToWorld = translate(cam.position) * toMat4(cam.orientation);
    const double err = maxDiff(cam.view() * cameraToWorld, Mat4());
    check("камера: матриця виду обернена до матриці положення й орієнтації", err < 1e-5, "похибка " + num(err));

    // Повні оберти в усіх площинах: орієнтація повертається до початкової
    Camera c2;
    c2.lookAtPoint(Vec3(0, 0, 10), Vec3(0, 0, 0));
    const Quat start = c2.orientation;
    for (int i = 0; i < 360; ++i) c2.rotateLocal(radians(1.0f), 0, 0);
    const Quat afterYaw = c2.orientation;
    for (int i = 0; i < 360; ++i) c2.rotateLocal(0, radians(1.0f), 0);
    const Quat afterPitch = c2.orientation;
    for (int i = 0; i < 360; ++i) c2.rotateLocal(0, 0, radians(1.0f));
    auto same = [](const Quat& a, const Quat& b) {
        return 1.0 - std::fabs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);
    };
    check("повний оберт 360° рисканням, тангажем і кренем повертає орієнтацію (без обмежень на кут)",
          same(start, afterYaw) < 1e-5 && same(start, afterPitch) < 1e-5 && same(start, c2.orientation) < 1e-5,
          "відхилення " + num(same(start, c2.orientation)));

    // Тангаж через полюс: камера дивиться вгору, а потім «догори дриґом» — базис лишається ортонормованим
    Camera c3;
    c3.lookAtPoint(Vec3(0, 0, 0), Vec3(0, 0, -1));
    for (int i = 0; i < 135; ++i) c3.rotateLocal(0, radians(1.0f), 0);
    const Vec3 f3 = c3.forward(), u3 = c3.up(), r3 = c3.right();
    check("тангаж 135° (через зеніт): вектор «вгору» камери спрямований назад, базис ортонормований",
          f3.y > 0.6f && u3.z > 0.6f && std::fabs(dot(f3, u3)) < 1e-5 && std::fabs(length(r3) - 1.0f) < 1e-5 &&
              dist(cross(r3, u3), -f3) < 1e-5,
          "forward.y = " + num(f3.y) + ", up.z = " + num(u3.z));

    Camera c4;
    c4.lookAtPoint(Vec3(0, 0, 0), Vec3(1, 0, 0));
    const Vec3 before = c4.position;
    c4.moveLocal(Vec3(0.5f, -1.0f, 2.0f));
    const Vec3 moved = c4.position - before;
    check("moveLocal: рух уздовж локальних осей (праворуч, вгору, уперед)",
          dist(moved, c4.right() * 0.5f - c4.up() * 1.0f + c4.forward() * 2.0f) < 1e-5 &&
              dist(c4.forward(), Vec3(1, 0, 0)) < 1e-5);

    Camera c5;
    const Vec3 target(10, 2, -5);
    double worst = 0.0;
    bool aimed = true;
    for (float az = 0.0f; az < 6.3f; az += 0.7f) {
        for (float el = -1.4f; el < 1.5f; el += 0.5f) {
            c5.orbit(target, 12.0f, az, el);
            worst = std::max(worst, std::fabs(dist(c5.position, target) - 12.0));
            aimed = aimed && dot(c5.forward(), normalize(target - c5.position)) > 0.9999f;
        }
    }
    c5.orbit(target, 12.0f, 0.0f, 3.0f);   // понад 90° — кут обмежується
    const bool clamped = c5.position.y - target.y < 12.0f * std::sin(radians(89.5f)) + 1e-3f;
    check("орбітальний режим: відстань до цілі стала, камера дивиться на ціль, висота обмежена 89°",
          worst < 1e-3 && aimed && clamped, "розбіжність відстані " + num(worst));
}

// ---------------------------------------------------------------------------
// Сцена
// ---------------------------------------------------------------------------
void testScene() {
    SolarSystem sys;
    const int sun = sys.indexOf("sun"), earth = sys.indexOf("earth"), moon = sys.indexOf("moon");
    check("склад сцени: Сонце, 8 планет, 6 супутників, корабель, пояс астероїдів",
          sys.bodyCount() == 15 && sun == 0 && sys.indexOf("neptune") == 14 && sys.selectableCount() == 16 &&
              sys.asteroidMatrices().size() == 2600,
          num(sys.bodyCount(), "%.0f") + " тіл, " + num(static_cast<double>(sys.asteroidMatrices().size()), "%.0f") +
              " астероїдів");

    // Ієрархія: положення Місяця = положення Землі + Ry(θ) · (r; 0; 0)
    const double times[] = {0.0, 13.7, 91.3, 400.0, 2750.0};
    double worstHier = 0.0, worstRadius = 0.0;
    for (double t : times) {
        sys.update(t);
        const BodyDef& md = sys.def(moon);
        const double theta = radians(md.phase0) + 2.0 * kPi * t / md.orbitPeriod;
        const Vec3 expected = sys.state(earth).position +
                              Vec3(static_cast<float>(md.orbitRadius * std::cos(theta)), 0.0f,
                                   static_cast<float>(-md.orbitRadius * std::sin(theta)));
        worstHier = std::max(worstHier, dist(sys.state(moon).position, expected));
        for (int i = 1; i < sys.bodyCount(); ++i) {
            const BodyDef& d = sys.def(i);
            worstRadius = std::max(worstRadius, std::fabs(dist(sys.state(i).position, sys.state(d.parent).position) - d.orbitRadius));
        }
    }
    check("ієрархія: Місяць = Земля + Ry(θ)·(r; 0; 0); всі тіла на сталій відстані від батьківського",
          worstHier < 2e-3 && worstRadius < 2e-3, "похибки " + num(worstHier) + " і " + num(worstRadius));

    // Період обігу
    double worstPeriod = 0.0;
    sys.update(37.0);
    std::vector<Vec3> rel0;
    for (int i = 1; i < sys.bodyCount(); ++i) rel0.push_back(sys.state(i).position - sys.state(sys.def(i).parent).position);
    std::vector<Vec3> rel1;
    for (int i = 1; i < sys.bodyCount(); ++i) {
        const double T = sys.def(i).orbitPeriod;
        SolarSystem fresh;
        fresh.update(37.0 + T);
        worstPeriod = std::max(worstPeriod, dist(fresh.state(i).position - fresh.state(sys.def(i).parent).position, rel0[static_cast<size_t>(i - 1)]) /
                                                sys.def(i).orbitRadius);
    }
    check("через повний період кожне тіло повертається в те саме положення на орбіті", worstPeriod < 2e-4,
          "відносна похибка " + num(worstPeriod));

    // Нахил осі й власне обертання
    double worstTilt = 0.0, worstTilt2 = 0.0;
    SolarSystem s2;
    for (int i = 0; i < s2.bodyCount(); ++i) {
        s2.update(10.0);
        const Vec3 axisA = transformDirection(s2.state(i).model, Vec3(0, 1, 0));
        s2.update(777.0);
        const Vec3 axisB = transformDirection(s2.state(i).model, Vec3(0, 1, 0));
        worstTilt = std::max(worstTilt, std::fabs(angleBetween(axisA, Vec3(0, 1, 0)) - s2.def(i).axialTilt));
        worstTilt2 = std::max(worstTilt2, angleBetween(axisA, axisB));
    }
    check("нахил осі кожного тіла дорівнює заданому й не залежить від часу (сезони)", worstTilt < 0.02 && worstTilt2 < 0.01,
          "похибки " + num(worstTilt) + "° і " + num(worstTilt2) + "°");

    s2.update(100.0);
    const Vec3 v1 = normalize(transformDirection(s2.state(earth).model, Vec3(1, 0, 0)));
    const Vec3 ax = normalize(transformDirection(s2.state(earth).model, Vec3(0, 1, 0)));
    s2.update(101.7);
    const Vec3 v2 = normalize(transformDirection(s2.state(earth).model, Vec3(1, 0, 0)));
    const double got = std::atan2(dot(ax, cross(v1, v2)), dot(v1, v2));
    double want = std::fmod(2.0 * kPi * 1.7 * SolarSystem::kSpinScale / 1.0, 2.0 * kPi);
    if (want > kPi) want -= 2.0 * kPi;
    check("власне обертання Землі за 1,7 доби = 2π · 1,7 · 0,1 / 1 (за правилом правої руки)", std::fabs(got - want) < 2e-3,
          "кут " + num(got * 180 / kPi) + "° проти " + num(want * 180 / kPi) + "°");

    // Приливний захват Місяця й зворотне обертання Венери
    double worstLock = 0.0;
    for (double t : {0.0, 5.0, 11.1, 200.0}) {
        s2.update(t);
        const Vec3 toEarth = s2.state(earth).position - s2.state(moon).position;
        // локальна вісь x стоїть у площині екватора (нахил 1,5° ≈ 0): напрямок на Землю
        const Vec3 face = transformDirection(s2.state(moon).model, Vec3(1, 0, 0));
        worstLock = std::max(worstLock, angleBetween(face, toEarth));
    }
    s2.update(0.0);
    const Vec3 venusAxis = transformDirection(s2.state(s2.indexOf("venus")).model, Vec3(0, 1, 0));
    check("Місяць завжди повернений до Землі одним боком (≤ 2°); вісь Венери перекинута (зворотне обертання)",
          worstLock < 2.0 && venusAxis.y < -0.99f, "відхилення " + num(worstLock) + "°");

    // Розмір тіл: довжина стовпців матриці моделі
    double worstScale = 0.0;
    for (int i = 0; i < s2.bodyCount(); ++i) {
        const Vec3 cx = transformDirection(s2.state(i).model, Vec3(1, 0, 0));
        const Vec3 cy = transformDirection(s2.state(i).model, Vec3(0, 1, 0));
        worstScale = std::max(worstScale, static_cast<double>(std::fabs(length(cx) - s2.def(i).radius)));
        worstScale = std::max(worstScale, static_cast<double>(std::fabs(length(cy) - s2.def(i).radius)));
    }
    check("матриця моделі кожного тіла містить масштаб, що дорівнює радіусу", worstScale < 1e-4, "похибка " + num(worstScale));

    // Геометрія затемнень: Сонце–Земля–Місяць на одній прямій
    SolarSystem ecl;
    ecl.update(0.0);
    const Vec3 sunEarth0 = ecl.state(earth).position - ecl.state(sun).position;
    const Vec3 earthMoon0 = ecl.state(moon).position - ecl.state(earth).position;
    const double lunar = angleBetween(sunEarth0, earthMoon0);
    ecl.update(14.77);
    const Vec3 sunEarth1 = ecl.state(earth).position - ecl.state(sun).position;
    const Vec3 earthMoon1 = ecl.state(moon).position - ecl.state(earth).position;
    const double solar = angleBetween(sunEarth1, earthMoon1);
    check("при t = 0 Місяць позаду Землі (місячне затемнення), при t = 14,77 доби — між Сонцем і Землею (сонячне)",
          lunar < 1.5 && solar > 178.0, "кути " + num(lunar) + "° і " + num(solar) + "°");

    // Корабель
    double worstShip = 0.0, maxY = 0.0;
    for (double t = 0.0; t < 520.0; t += 13.0) {
        ecl.update(t);
        const ShipState& ship = ecl.ship();
        worstShip = std::max(worstShip, std::fabs(length(ship.position) - 44.0));
        worstShip = std::max(worstShip, static_cast<double>(std::fabs(dot(ship.forward, normalize(ship.position)))));
        worstShip = std::max(worstShip, dist(normalize(transformDirection(ship.model, Vec3(0, 0, -1))), ship.forward));
        maxY = std::max(maxY, static_cast<double>(std::fabs(ship.position.y)));
    }
    check("корабель: коло радіуса 44 у площині з нахилом 8°, ніс уздовж дотичної", worstShip < 1e-3 &&
              std::fabs(maxY - 44.0 * std::sin(radians(8.0f))) < 0.1,
          "найбільша висота " + num(maxY));

    double rmin = 1e9, rmax = 0.0, ymax = 0.0;
    for (const Mat4& m : ecl.asteroidMatrices()) {
        const double r = std::sqrt(m.at(0, 3) * m.at(0, 3) + m.at(2, 3) * m.at(2, 3));
        rmin = std::min(rmin, r);
        rmax = std::max(rmax, r);
        ymax = std::max(ymax, static_cast<double>(std::fabs(m.at(1, 3))));
    }
    check("пояс астероїдів лежить між Марсом і Юпітером: радіуси в межах [49; 59]",
          rmin >= SolarSystem::kBeltInner - 1e-3 && rmax <= SolarSystem::kBeltOuter + 1e-3 && ymax < 3.3,
          "[" + num(rmin) + "; " + num(rmax) + "]");
}

// ---------------------------------------------------------------------------
// Геометрія й ресурси
// ---------------------------------------------------------------------------
double winding(const MeshData& m) {   // частка трикутників, чия нормаль збігається з нормалями вершин
    size_t good = 0;
    size_t degenerate = 0;
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const Vertex& a = m.vertices[m.indices[i]];
        const Vertex& b = m.vertices[m.indices[i + 1]];
        const Vertex& c = m.vertices[m.indices[i + 2]];
        const Vec3 n = cross(b.position - a.position, c.position - a.position);
        if (length(n) < 1e-9f) {   // вироджені трикутники біля полюсів не враховуються
            ++degenerate;
            continue;
        }
        if (dot(n, a.normal + b.normal + c.normal) > 0.0f) ++good;
    }
    return static_cast<double>(good) / std::max<size_t>(1, m.indices.size() / 3 - degenerate);
}

void testGeometry() {
    const MeshData sph = makeSphere(24, 48);
    double worstN = 0.0, worstT = 0.0, worstP = 0.0, uvMin = 1.0, uvMax = 0.0;
    for (const Vertex& v : sph.vertices) {
        worstN = std::max(worstN, static_cast<double>(std::fabs(length(v.normal) - 1.0f)));
        worstT = std::max(worstT, static_cast<double>(std::fabs(dot(v.normal, v.tangent))));
        worstP = std::max(worstP, static_cast<double>(std::fabs(length(v.position) - 1.0f)));
        uvMin = std::min({uvMin, static_cast<double>(v.uv.x), static_cast<double>(v.uv.y)});
        uvMax = std::max({uvMax, static_cast<double>(v.uv.x), static_cast<double>(v.uv.y)});
    }
    check("сфера: (stacks + 1)(slices + 1) вершин, одиничні нормалі й радіус, дотична ⟂ нормалі, uv ∈ [0; 1], обхід назовні",
          sph.vertices.size() == 25u * 49u && sph.indices.size() == 24u * 48u * 6u && worstN < 1e-5 && worstT < 1e-5 &&
              worstP < 1e-5 && uvMin >= 0.0 && uvMax <= 1.0 && winding(sph) > 0.999,
          num(static_cast<double>(sph.vertices.size()), "%.0f") + " вершин, " +
              num(static_cast<double>(sph.indices.size() / 3), "%.0f") + " трикутників");

    const MeshData ring = makeRing(1.28f, 2.35f, 64);
    bool ringOk = true;
    for (size_t i = 0; i + 2 < ring.indices.size(); i += 3) {
        const Vec3 n = cross(ring.vertices[ring.indices[i + 1]].position - ring.vertices[ring.indices[i]].position,
                             ring.vertices[ring.indices[i + 2]].position - ring.vertices[ring.indices[i]].position);
        ringOk = ringOk && n.y > 0.0f;
    }
    double rmin = 1e9, rmax = 0.0;
    for (const Vertex& v : ring.vertices) {
        const double r = std::sqrt(v.position.x * v.position.x + v.position.z * v.position.z);
        rmin = std::min(rmin, r);
        rmax = std::max(rmax, r);
    }
    check("кільце: площина xOz, радіуси [1,28; 2,35], обхід проти годинникової стрілки зверху (+y)",
          ringOk && std::fabs(rmin - 1.28) < 1e-4 && std::fabs(rmax - 2.35) < 1e-4);

    // OBJ: чотирикутник, відʼємні індекси
    const std::string quad = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                             "f 1/1 2/2 3/3 4/4\n";
    MeshData m1;
    std::string error;
    const bool ok1 = parseObj(quad, m1, &error);
    const std::string negative = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n";
    MeshData m2;
    const bool ok2 = parseObj(negative, m2, &error);
    check("OBJ: чотирикутник розбивається на 2 трикутники, нормаль обчислюється, дотична збігається з осю u",
          ok1 && m1.vertices.size() == 4 && m1.indices.size() == 6 && dist(m1.vertices[0].normal, Vec3(0, 0, 1)) < 1e-5 &&
              dist(m1.vertices[0].tangent, Vec3(1, 0, 0)) < 1e-5,
          "вершин: " + num(static_cast<double>(m1.vertices.size()), "%.0f"));
    MeshData bad;
    const bool okBad = parseObj("v 0 0 0\nf 1 2 3\n", bad, &error);
    check("OBJ: від'ємні індекси читаються; індекс поза межами розпізнається як помилка",
          ok2 && m2.indices.size() == 3 && !okBad && !error.empty(), "повідомлення: " + error);

    // Моделі з assets
    for (const char* name : {"ship.obj", "asteroid.obj"}) {
        MeshData mesh;
        std::string err;
        const bool ok = loadObj(assets::path(std::string("models/") + name), mesh, &err);
        std::string text;
        assets::readText(assets::path(std::string("models/") + name), text);
        size_t faces = 0;
        for (size_t pos = 0; (pos = text.find("\nf ", pos)) != std::string::npos; ++pos) ++faces;
        double wn = 0.0, wt = 0.0, wo = 0.0;
        for (const Vertex& v : mesh.vertices) {
            wn = std::max(wn, static_cast<double>(std::fabs(length(v.normal) - 1.0f)));
            wt = std::max(wt, static_cast<double>(std::fabs(dot(v.normal, v.tangent))));
            wo = std::max(wo, static_cast<double>(std::fabs(length(v.tangent) - 1.0f)));
        }
        check(std::string("модель ") + name + ": кількість трикутників збігається з числом граней файла, нормалі й дотичні одиничні, обхід назовні",
              ok && mesh.indices.size() == faces * 3 && wn < 1e-3 && wt < 1e-3 && wo < 1e-3 && winding(mesh) > 0.99,
              num(static_cast<double>(faces), "%.0f") + " граней, " + num(static_cast<double>(mesh.vertices.size()), "%.0f") + " вершин");
    }
}

std::vector<unsigned char> tgaHeader(int type, int w, int h, int depth, int descriptor) {
    std::vector<unsigned char> b(18, 0);
    b[2] = static_cast<unsigned char>(type);
    b[12] = static_cast<unsigned char>(w & 255);
    b[14] = static_cast<unsigned char>(h & 255);
    b[16] = static_cast<unsigned char>(depth);
    b[17] = static_cast<unsigned char>(descriptor);
    return b;
}

void testAssets() {
    // TGA: початок координат унизу ліворуч, RLE, коло «запис → читання»
    std::vector<unsigned char> a = tgaHeader(2, 2, 2, 24, 0);
    const unsigned char bottom[] = {255, 0, 0, 255, 255, 255};    // BGR: синій, білий (нижній рядок першим)
    const unsigned char top[] = {0, 0, 255, 0, 255, 0};          // червоний, зелений
    a.insert(a.end(), bottom, bottom + 6);
    a.insert(a.end(), top, top + 6);
    Image i1;
    std::string err;
    const bool ok1 = decodeTga(a, i1, &err);
    const bool layout = ok1 && i1.width == 2 && i1.height == 2 && i1.channels == 3 && i1.pixel(0, 0)[0] == 255 &&
                        i1.pixel(0, 0)[1] == 0 && i1.pixel(1, 0)[1] == 255 && i1.pixel(0, 1)[2] == 255 &&
                        i1.pixel(1, 1)[0] == 255 && i1.pixel(1, 1)[2] == 255;

    std::vector<unsigned char> r = tgaHeader(10, 4, 1, 24, 0x20);
    r.push_back(0x83);   // пакет повтору: 4 пікселі
    r.insert(r.end(), {10, 20, 30});
    Image i2;
    const bool ok2 = decodeTga(r, i2, &err);
    const bool rle = ok2 && i2.width == 4 && i2.pixel(3, 0)[0] == 30 && i2.pixel(3, 0)[2] == 10;

    Image src;
    src.width = 5;
    src.height = 3;
    src.channels = 4;
    for (int i = 0; i < 5 * 3 * 4; ++i) src.pixels.push_back(static_cast<unsigned char>((i * 37 + 11) & 255));
    const std::filesystem::path tmp = std::filesystem::temp_directory_path() / "selftest_roundtrip.tga";
    Image back;
    const bool saved = saveTga(tmp, src);
    const bool loaded = saved && loadTga(tmp, back, &err);
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    const bool round = loaded && back.pixels == src.pixels && back.channels == 4;

    std::vector<unsigned char> cut = a;
    cut.resize(cut.size() - 5);
    Image i3;
    const bool okCut = decodeTga(cut, i3, &err);
    check("TGA: розташування початку координат, RLE, повний цикл «запис → читання» без втрат; обірваний файл відхиляється",
          layout && rle && round && !okCut, "помилка обірваного файла: " + err);

    // Усі файли ресурсів існують і мають очікувану форму
    struct Spec { const char* file; int w; int h; int ch; };
    const Spec specs[] = {{"sun.tga", 1024, 512, 3}, {"mercury.tga", 1024, 512, 3}, {"mercury_n.tga", 1024, 512, 3},
                          {"venus.tga", 1024, 512, 3}, {"earth.tga", 2048, 1024, 3}, {"earth_s.tga", 2048, 1024, 1},
                          {"moon.tga", 1024, 512, 3}, {"moon_n.tga", 1024, 512, 3}, {"mars.tga", 1024, 512, 3},
                          {"mars_n.tga", 1024, 512, 3}, {"jupiter.tga", 1024, 512, 3}, {"io.tga", 512, 256, 3},
                          {"europa.tga", 512, 256, 3}, {"ganymede.tga", 512, 256, 3}, {"callisto.tga", 512, 256, 3},
                          {"saturn.tga", 1024, 512, 3}, {"saturn_ring.tga", 1024, 16, 4}, {"titan.tga", 512, 256, 3},
                          {"uranus.tga", 1024, 512, 3}, {"neptune.tga", 1024, 512, 3}, {"ship.tga", 512, 512, 3},
                          {"rock.tga", 512, 256, 3}, {"sky_px.tga", 1024, 1024, 3}, {"sky_nx.tga", 1024, 1024, 3},
                          {"sky_py.tga", 1024, 1024, 3}, {"sky_ny.tga", 1024, 1024, 3}, {"sky_pz.tga", 1024, 1024, 3},
                          {"sky_nz.tga", 1024, 1024, 3}};
    int missing = 0, wrong = 0;
    std::string firstBad;
    for (const Spec& s : specs) {
        Image img;
        if (!loadTga(assets::path(std::string("textures/") + s.file), img, nullptr)) {
            ++missing;
            if (firstBad.empty()) firstBad = s.file;
        } else if (img.width != s.w || img.height != s.h || img.channels != s.ch) {
            ++wrong;
            if (firstBad.empty()) firstBad = s.file;
        }
    }
    check("ресурси: усі 28 текстур завантажуються й мають очікувані розміри та кількість каналів",
          missing == 0 && wrong == 0, "перша проблема: " + (firstBad.empty() ? std::string("немає") : firstBad));

    // Вміст: небо темне зі зорями, Сонце яскраве, карта нормалей має одиничні вектори, прозорість кільця змінюється
    Image sky, sun, nrm, ring, earthS;
    loadTga(assets::path("textures/sky_py.tga"), sky);
    loadTga(assets::path("textures/sun.tga"), sun);
    loadTga(assets::path("textures/moon_n.tga"), nrm);
    loadTga(assets::path("textures/saturn_ring.tga"), ring);
    loadTga(assets::path("textures/earth_s.tga"), earthS);
    auto meanMax = [](const Image& im, double& mean, int& mx) {
        double s = 0;
        mx = 0;
        for (size_t i = 0; i < im.pixels.size(); ++i) {
            s += im.pixels[i];
            mx = std::max(mx, static_cast<int>(im.pixels[i]));
        }
        mean = s / im.pixels.size() / 255.0;
    };
    double skyMean = 0, sunMean = 0;
    int skyMax = 0, sunMax = 0;
    meanMax(sky, skyMean, skyMax);
    meanMax(sun, sunMean, sunMax);
    double nlen = 0;
    for (size_t i = 0; i + 2 < nrm.pixels.size(); i += 97 * 3) {
        const double x = nrm.pixels[i] / 127.5 - 1, y = nrm.pixels[i + 1] / 127.5 - 1, z = nrm.pixels[i + 2] / 127.5 - 1;
        nlen = std::max(nlen, std::fabs(std::sqrt(x * x + y * y + z * z) - 1.0));
    }
    int amin = 255, amax = 0;
    for (size_t i = 3; i < ring.pixels.size(); i += 4) {
        amin = std::min(amin, static_cast<int>(ring.pixels[i]));
        amax = std::max(amax, static_cast<int>(ring.pixels[i]));
    }
    int smin = 255, smax = 0;
    for (unsigned char v : earthS.pixels) {
        smin = std::min(smin, static_cast<int>(v));
        smax = std::max(smax, static_cast<int>(v));
    }
    check("вміст текстур: небо темне зі зорями, Сонце світле, нормалі одиничні, кільце має щілини, карта дзеркальності розрізняє суходіл і океан",
          skyMean < 0.12 && skyMax > 200 && sunMean > 0.35 && nlen < 0.04 && amin < 20 && amax > 180 && smin < 30 && smax > 200,
          "небо " + num(skyMean) + ", Сонце " + num(sunMean) + ", відхил. нормалей " + num(nlen));
}

// ---------------------------------------------------------------------------
// Відтворення
// ---------------------------------------------------------------------------
struct Ctx {
    SolarSystem system;
    Renderer renderer;
};

double luma(const unsigned char* p) { return (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) / 255.0; }

double meanLumaDisc(const Image& im, double cx, double cy, double radius) {
    double sum = 0;
    int count = 0;
    for (int y = std::max(0, static_cast<int>(cy - radius)); y < std::min(im.height, static_cast<int>(cy + radius) + 1); ++y) {
        for (int x = std::max(0, static_cast<int>(cx - radius)); x < std::min(im.width, static_cast<int>(cx + radius) + 1); ++x) {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius) {
                sum += luma(im.pixel(x, y));
                ++count;
            }
        }
    }
    return count > 0 ? sum / count : 0.0;
}

struct Projected {
    double x = 0;
    double y = 0;
    double radius = 0;
    bool visible = false;
};

Projected project(const Camera& cam, const Vec3& world, float bodyRadius, int w, int h) {
    const Mat4 vp = cam.projection(static_cast<float>(w) / h) * cam.view();
    const Vec4 c = vp * Vec4(world, 1.0f);
    Projected p;
    p.visible = c.w > 0.0f && std::fabs(c.x / c.w) < 1.0f && std::fabs(c.y / c.w) < 1.0f;
    p.x = (c.x / c.w * 0.5 + 0.5) * w;
    p.y = (1.0 - (c.y / c.w * 0.5 + 0.5)) * h;
    const double focal = (h * 0.5) / std::tan(cam.fovY * 0.5);
    // Тіло позаду камери або на її площині не має кінцевого розміру на екрані; без обмеження циклі за
    // променями від центру диска були б майже нескінченними, якщо матриця виду зіпсована.
    p.radius = c.w > 1e-3f ? std::min(bodyRadius / static_cast<double>(c.w) * focal, 1.0e5) : 0.0;
    return p;
}

RenderSettings plain() {
    RenderSettings s;
    s.hud.visible = false;
    s.outline = false;
    s.glow = false;
    s.orbits = false;
    s.belt = false;
    s.ship = false;
    return s;
}

Image frame(Ctx& ctx, const Camera& cam, const RenderSettings& s, int w, int h, int samples = 1) {
    Offscreen target;
    if (!target.create(w, h, samples)) {
        return Image();
    }
    RenderTarget rt;
    rt.framebuffer = target.framebuffer();
    rt.width = w;
    rt.height = h;
    ctx.renderer.render(ctx.system, cam, s, rt);
    return target.read();
}

Camera presetCamera(Ctx& ctx, const std::string& name, double days) {
    ctx.system.update(days);
    Camera cam;
    applyPreset(*findPreset(name), ctx.system, cam);
    return cam;
}

void testShaders() {
    struct Spec { const char* v; const char* f; const char* g; };
    const Spec specs[] = {{"planet.vert", "planet.frag", ""}, {"planet.vert", "sun.frag", ""},
                          {"skybox.vert", "skybox.frag", ""}, {"flat.vert", "flat.frag", ""},
                          {"shadow.vert", "shadow.frag", "shadow.geom"}, {"normals.vert", "normals.frag", "normals.geom"},
                          {"glow.vert", "glow.frag", ""}, {"hud.vert", "hud.frag", ""}};
    int built = 0, geometry = 0;
    std::string firstError;
    for (const Spec& s : specs) {
        Shader sh;
        std::string error;
        if (sh.build(s.v, s.f, s.g, &error)) {
            ++built;
            geometry += sh.hasGeometryStage() ? 1 : 0;
        } else if (firstError.empty()) {
            firstError = error;
        }
    }
    check("усі 8 шейдерних програм компілюються й компонуються; геометричний етап мають дві (тіні, нормалі)",
          built == 8 && geometry == 2, firstError.empty() ? "без помилок" : firstError);

    Shader broken;
    std::string error;
    const bool okBroken = broken.buildFromSource("#version 330 core\nvoid main() { gl_Position = vec4(0.0); }\n",
                                                 "#version 330 core\nout vec4 c;\nvoid main() { c = undefinedName; }\n", "", &error);
    check("помилка в шейдері розпізнається, журнал компілятора повертається", !okBroken && !error.empty(),
          error.substr(0, error.find('\n') == std::string::npos ? error.size() : error.find('\n')));
}

// Перший рядок зображення — верх кадру: текстура з білим верхнім і чорним нижнім рядком малюється на весь екран
void testTextureOrientation() {
    Image img;
    img.width = 2;
    img.height = 2;
    img.channels = 3;
    img.pixels = {255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0};
    const GLuint tex = createTexture2D(img, false);
    Shader sh;
    std::string error;
    const bool built = sh.buildFromSource(
        "#version 330 core\nlayout(location = 0) in vec3 aPosition;\nlayout(location = 2) in vec2 aUV;\n"
        "out vec2 vUV;\nvoid main() { vUV = aUV; gl_Position = vec4(aPosition, 1.0); }\n",
        "#version 330 core\nin vec2 vUV;\nout vec4 c;\nuniform sampler2D uTex;\n"
        "void main() { c = vec4(texture(uTex, vUV).rgb, 1.0); }\n", "", &error);
    Mesh quad;
    quad.upload(makeQuad());
    Offscreen target;
    bool top = false, bottom = false;
    if (built && target.create(16, 16, 1)) {
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer());
        glViewport(0, 0, 16, 16);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        sh.use();
        sh.set("uTex", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        quad.draw();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        const Image im = target.read();
        top = im.pixel(8, 2)[0] > 200;
        bottom = im.pixel(8, 13)[0] < 60;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tex);
    check("орієнтація текстур: перший рядок зображення потрапляє у верх кадру (v = 1), останній — унизу", built && top && bottom,
          error);
}

void testPresets(Ctx& ctx) {
    int bad = 0;
    std::string which;
    for (const ViewPreset& p : viewPresets()) {
        ctx.system.update(p.days);
        Camera cam;
        applyPreset(p, ctx.system, cam);
        const bool finite = std::isfinite(cam.position.x) && std::isfinite(cam.position.y) && std::isfinite(cam.position.z);
        bool inView = true;
        if (!p.selectedId.empty()) {
            const int idx = p.selectedId == "ship" ? ctx.system.shipIndex() : ctx.system.indexOf(p.selectedId);
            const Projected pr = project(cam, ctx.system.targetPosition(idx), ctx.system.targetRadius(idx), 1280, 720);
            inView = pr.visible && pr.radius > 2.0;
        }
        const double orth = std::fabs(length(cam.forward()) - 1.0) + std::fabs(dot(cam.forward(), cam.up()));
        if (!finite || !inView || orth > 1e-4) {
            ++bad;
            if (which.empty()) which = p.name;
        }
    }
    check("готові ракурси: камера коректна, вибране тіло в кадрі й має помітний розмір", bad == 0,
          bad == 0 ? num(static_cast<double>(viewPresets().size()), "%.0f") + " ракурсів" : "проблема: " + which);
}

void testRendering(Ctx& ctx) {
    // Затемнення Місяця: у тіні Землі він темніший у кілька разів
    Camera cam = presetCamera(ctx, "lunar-eclipse", 0.0);
    RenderSettings on = plain();
    RenderSettings off = plain();
    off.shadows = false;
    const int W = 640, H = 360;
    const Image withShadow = frame(ctx, cam, on, W, H);
    const Image noShadow = frame(ctx, cam, off, W, H);
    const int moon = ctx.system.indexOf("moon");
    const Projected pm = project(cam, ctx.system.state(moon).position, ctx.system.def(moon).radius, W, H);
    const double lOn = meanLumaDisc(withShadow, pm.x, pm.y, pm.radius * 0.7);
    const double lOff = meanLumaDisc(noShadow, pm.x, pm.y, pm.radius * 0.7);
    check("місячне затемнення: Місяць у тіні Землі темніший щонайменше в 2,5 раза (карта тіней із геометричним шейдером)",
          pm.visible && pm.radius > 8 && lOff > 0.08 && lOn < lOff / 2.5 && lOn < 0.12,
          "яскравість з тінню " + num(lOn, "%.3f") + ", без тіні " + num(lOff, "%.3f"));

    // Сонячне затемнення: на Землі з'являється темна пляма, що лежить усередині диска
    cam = presetCamera(ctx, "solar-eclipse", 14.77);
    const Image sOn = frame(ctx, cam, on, W, H);
    const Image sOff = frame(ctx, cam, off, W, H);
    const int earth = ctx.system.indexOf("earth");
    const Projected pe = project(cam, ctx.system.state(earth).position, ctx.system.def(earth).radius, W, H);
    int darkened = 0, outside = 0;
    double sx = 0, sy = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const double a = luma(sOn.pixel(x, y)), b = luma(sOff.pixel(x, y));
            if (b > 0.06 && a < b * 0.5) {
                const double dx = x - pe.x, dy = y - pe.y;
                if (dx * dx + dy * dy <= pe.radius * pe.radius) {
                    ++darkened;
                    sx += x;
                    sy += y;
                } else {
                    ++outside;
                }
            }
        }
    }
    const double cx = darkened > 0 ? sx / darkened : 0, cy = darkened > 0 ? sy / darkened : 0;
    const double fromCenter = std::sqrt((cx - pe.x) * (cx - pe.x) + (cy - pe.y) * (cy - pe.y)) / pe.radius;
    check("сонячне затемнення: тінь Місяця — темна пляма на диску Землі (≥ 150 пікселів), поза Землею тінь нічого не змінює",
          darkened >= 150 && fromCenter < 0.95 && outside < darkened / 3,
          num(darkened, "%.0f") + " пікселів, центр плями на " + num(fromCenter) + " радіуса від центру");

    // Тінь Сатурна на його кільці (і кільця на планеті): кадр із тінями темніший у помітній області
    cam = presetCamera(ctx, "saturn", 3287.0);
    const Image satOn = frame(ctx, cam, on, W, H);
    const Image satOff = frame(ctx, cam, off, W, H);
    const int saturn = ctx.system.indexOf("saturn");
    const Projected ps = project(cam, ctx.system.state(saturn).position, ctx.system.def(saturn).radius, W, H);
    int ringDark = 0, farAway = 0, ringLit = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const double a = luma(satOn.pixel(x, y)), b = luma(satOff.pixel(x, y));
            const double dx = x - ps.x, dy = y - ps.y;
            const bool closeBy = dx * dx + dy * dy < 9.0 * ps.radius * ps.radius;
            if (closeBy && b > 0.08) ++ringLit;
            if (b > 0.08 && a < b * 0.6) {
                if (closeBy) ++ringDark;
                else ++farAway;
            }
        }
    }
    check("кільце Сатурна: тінь планети лягає на кільце (≥ 400 затемнених пікселів), далеко від планети тіні немає",
          ringDark >= 400 && farAway < 20, num(ringDark, "%.0f") + " пікселів, поза кільцем " + num(farAway, "%.0f"));
    check("кільце Сатурна не затінює саме себе: затемнено менше чверті освітлених пікселів (тінь планети займає лише клин)",
          ringDark * 4 < ringLit, num(ringDark, "%.0f") + " із " + num(ringLit, "%.0f") + " освітлених");

    // Тінь кільця на самому Сатурні: камера нижче площини кільця, з боку Сонця — на південній півкулі видно смугу тіні
    {
        const Vec3 sp = ctx.system.state(saturn).position;
        Camera below;
        below.lookAtPoint(sp + Vec3(-18.0f, -9.0f, 15.0f), sp);
        const Image rOn = frame(ctx, below, on, W, H);
        const Image rOff = frame(ctx, below, off, W, H);
        const Projected pg = project(below, sp, ctx.system.def(saturn).radius, W, H);
        int shaded = 0;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const double dx = x - pg.x, dy = y - pg.y;
                if (dx * dx + dy * dy < pg.radius * pg.radius) {
                    const double a = luma(rOn.pixel(x, y)), b = luma(rOff.pixel(x, y));
                    if (b > 0.08 && a < b * 0.6) ++shaded;
                }
            }
        }
        check("кільце Сатурна відкидає тінь на планету: на диску з'являється темна смуга (≥ 300 пікселів)", shaded >= 300,
              num(shaded, "%.0f") + " пікселів");
    }

    // Освітлена півкуля не затінює сама себе: у кадрі Земля + Місяць (затемнення немає) тіні майже нічого не змінюють
    {
        const Camera cc = presetCamera(ctx, "earth-close", 100.0);
        const Image cOn = frame(ctx, cc, on, W, H);
        const Image cOff = frame(ctx, cc, off, W, H);
        int changed = 0, lit = 0;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const double a = luma(cOn.pixel(x, y)), b = luma(cOff.pixel(x, y));
                if (b > 0.08) ++lit;
                if (b > 0.08 && std::fabs(a - b) > 0.12 * b) ++changed;
            }
        }
        check("самотінення відсутнє біля термінатора: у кадрі Землі з Місяцем тіні змінюють яскравість менш ніж 0,05 % освітлених пікселів",
              changed * 2000 < lit, num(changed, "%.0f") + " із " + num(lit, "%.0f") + " освітлених");
    }
    {
        const Camera cE = presetCamera(ctx, "earth", 100.0);
        const Projected pE = project(cE, ctx.system.state(earth).position, ctx.system.def(earth).radius, W, H);
        const double a = meanLumaDisc(frame(ctx, cE, on, W, H), pE.x, pE.y, pE.radius * 0.85);
        const double b = meanLumaDisc(frame(ctx, cE, off, W, H), pE.x, pE.y, pE.radius * 0.85);
        check("самотінення відсутнє: яскравість диска Землі з тінями й без них розрізняється менш ніж на 3 %",
              std::fabs(a - b) / b < 0.03, "з тінями " + num(a, "%.4f") + ", без " + num(b, "%.4f"));
    }

    // Контур вибраного тіла (трафарет): на кожному з 24 променів від центру диска шукаємо помаранчевий піксель
    cam = presetCamera(ctx, "earth", 100.0);
    RenderSettings sel = plain();
    sel.outline = true;
    sel.selected = earth;
    const int OW = 1280, OH = 720;
    const Image withOutline = frame(ctx, cam, sel, OW, OH);
    sel.outline = false;
    const Image withoutOutline = frame(ctx, cam, sel, OW, OH);
    const Projected pr = project(cam, ctx.system.state(earth).position, ctx.system.def(earth).radius, OW, OH);
    auto isOrange = [](const unsigned char* p) { return p[0] > 200 && p[1] > 90 && p[1] < 190 && p[2] < 90; };
    auto rayHasOrange = [&](const Image& im, double a) {
        for (double r = pr.radius * 0.7; r < pr.radius * 1.4; r += 0.5) {
            const int x = static_cast<int>(std::lround(pr.x + r * std::cos(a)));
            const int y = static_cast<int>(std::lround(pr.y + r * std::sin(a)));
            if (x >= 0 && x < im.width && y >= 0 && y < im.height && isOrange(im.pixel(x, y))) return r;
        }
        return -1.0;
    };
    int ringOn = 0, ringOff = 0;
    double gapMin = 1e9, gapMax = 0.0;
    for (int k = 0; k < 24; ++k) {
        const double a = 2.0 * kPi * k / 24.0;
        const double hit = rayHasOrange(withOutline, a);
        if (hit > 0) {
            ++ringOn;
            gapMin = std::min(gapMin, hit / pr.radius);
            gapMax = std::max(gapMax, hit / pr.radius);
        }
        if (rayHasOrange(withoutOutline, a) > 0) ++ringOff;
    }
    const bool centerFree = !isOrange(withOutline.pixel(static_cast<int>(pr.x), static_cast<int>(pr.y)));
    check("контур за трафаретом: помаранчеве кільце навколо вибраного тіла (на всіх 24 променях), поза диском; усередині й без контуру його немає",
          ringOn == 24 && ringOff == 0 && centerFree && gapMin > 0.95 && gapMax < 1.1,
          num(ringOn, "%.0f") + " з 24 променів, кільце на " + num(gapMin) + "…" + num(gapMax) + " радіуса");

    // Геометричний шейдер нормалей: 3 відрізки на кожен трикутник
    RenderSettings ns = plain();
    ns.normals = true;
    ns.selected = earth;
    const Image withNormals = frame(ctx, cam, ns, W, H);
    int green = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const unsigned char* p = withNormals.pixel(x, y);
            if (p[1] > 200 && p[0] < 120 && p[2] < 160) ++green;
        }
    }
    const unsigned expected = static_cast<unsigned>(ctx.renderer.sphereLowTriangles() * 3);
    check("геометричний шейдер нормалей створює рівно 3 відрізки на трикутник сітки й вони видимі",
          ctx.renderer.lastNormalsPrimitives() == expected && green > 100,
          num(ctx.renderer.lastNormalsPrimitives(), "%.0f") + " відрізків (очікувано " + num(expected, "%.0f") +
              "), зелених пікселів " + num(green, "%.0f"));

    // Кубічна карта: кожна грань має свій колір; напрямок огляду вибирає грань за стандартом OpenGL
    std::array<Image, 6> faces;
    const unsigned char colors[6][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {0, 255, 255}, {255, 0, 255}};
    const int FS = 64;   // сторона грані
    for (int f = 0; f < 6; ++f) {
        Image& face = faces[static_cast<size_t>(f)];
        face.width = FS;
        face.height = FS;
        face.channels = 3;
        face.pixels.assign(static_cast<size_t>(FS) * FS * 3, 0);
        for (int y = 0; y < FS; ++y) {
            for (int x = 0; x < FS; ++x) {
                for (int c = 0; c < 3; ++c) {
                    unsigned char v = colors[f][c];
                    if (f == 4) {   // грань +z: верхній лівий квадрант білий, решта сіра
                        v = (x < FS / 2 && y < FS / 2) ? 255 : 90;
                    }
                    face.pixels[static_cast<size_t>((y * FS + x) * 3 + c)] = v;
                }
            }
        }
    }
    const GLuint synthetic = createCubemap(faces, false);
    const GLuint realSky = ctx.renderer.replaceSky(synthetic);
    const Vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    int faceOk = 0;
    std::string failed;
    for (int f = 0; f < 6; ++f) {
        Camera c;
        c.fovY = radians(20.0f);
        const Vec3 eye(600, 600, 600);
        const Vec3 upHint = (f == 2 || f == 3) ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
        c.lookAtPoint(eye, eye + dirs[f], upHint);
        RenderSettings s = plain();
        s.shadows = false;
        const Image im = frame(ctx, c, s, 64, 64);
        const unsigned char* px = im.pixel(32, 32);
        if (f == 4) {
            // Вид уздовж +z з «вгору» = +y: праворуч на екрані — −x, тож верхній лівий квадрант грані (x < 0)
            // має опинитися у верхній ПРАВІЙ частині: так влаштований стандарт кубічних карт OpenGL
            const unsigned char* tr = im.pixel(48, 16);
            const unsigned char* tl = im.pixel(16, 16);
            const unsigned char* br = im.pixel(48, 48);
            if (tr[0] > 230 && tr[1] > 230 && tr[2] > 230 && tl[0] > 120 && tl[0] < 215 && br[0] > 120 && br[0] < 215) ++faceOk;
            else if (failed.empty()) {
                failed = "+z (орієнтація): праворуч-вгору " + std::to_string(tr[0]) + ", ліворуч-вгору " + std::to_string(tl[0]) +
                         ", праворуч-вниз " + std::to_string(br[0]);
            }
        } else if (std::abs(static_cast<int>(px[0]) - colors[f][0]) < 14 && std::abs(static_cast<int>(px[1]) - colors[f][1]) < 14 &&
                   std::abs(static_cast<int>(px[2]) - colors[f][2]) < 14) {
            ++faceOk;
        } else if (failed.empty()) {
            failed = "грань " + std::to_string(f);
        }
    }
    ctx.renderer.replaceSky(realSky);
    glDeleteTextures(1, &synthetic);
    check("кубічна карта: за кожного з шести напрямків огляду видно свою грань, орієнтація в межах грані відповідає стандарту OpenGL",
          faceOk == 6, num(faceOk, "%.0f") + " з 6" + (failed.empty() ? "" : ", збій: " + failed));

    // Інстансинг: пояс астероїдів за один виклик
    cam = presetCamera(ctx, "belt", 120.0);
    RenderSettings beltOn = plain();
    beltOn.belt = true;
    RenderSettings beltOff = plain();
    const Image bOn = frame(ctx, cam, beltOn, W, H);
    const Image bOff = frame(ctx, cam, beltOff, W, H);
    long diff = 0;
    for (size_t i = 0; i < bOn.pixels.size(); i += 3) {
        if (std::abs(static_cast<int>(bOn.pixels[i + 1]) - static_cast<int>(bOff.pixels[i + 1])) > 25) ++diff;
    }
    check("інстансинг: 2600 астероїдів малюються одним викликом і з'являються на кадрі (≥ 2000 змінених пікселів)",
          ctx.renderer.beltInstances() == 2600 && diff >= 2000, num(static_cast<double>(diff), "%.0f") + " пікселів");

    // Освітлення: три джерела
    cam = presetCamera(ctx, "earth", 100.0);
    const Projected pe2 = project(cam, ctx.system.state(earth).position, ctx.system.def(earth).radius, W, H);
    RenderSettings lights = plain();
    lights.shadows = false;
    const double sunOn = meanLumaDisc(frame(ctx, cam, lights, W, H), pe2.x, pe2.y, pe2.radius * 0.8);
    lights.sunLight = false;
    const double sunOff = meanLumaDisc(frame(ctx, cam, lights, W, H), pe2.x, pe2.y, pe2.radius * 0.8);
    check("джерело 1 (Сонце, точкове): без нього диск Землі в кілька разів темніший, але не чорний (напрямлене світло й фон)",
          sunOn > 4.0 * sunOff && sunOff > 0.002, "з Сонцем " + num(sunOn, "%.3f") + ", без нього " + num(sunOff, "%.4f"));

    cam = presetCamera(ctx, "mars", 300.0);
    const int mars = ctx.system.indexOf("mars");
    const Projected pmars = project(cam, ctx.system.state(mars).position, ctx.system.def(mars).radius, W, H);
    RenderSettings torch = plain();
    torch.sunLight = false;
    const Image darkMars = frame(ctx, cam, torch, W, H);
    torch.torch = true;
    // ліхтар дивиться вздовж камери: наводимо його на Марс
    Camera torchCam = cam;
    torchCam.lookAtPoint(cam.position, ctx.system.state(mars).position);
    const Image litMars = frame(ctx, torchCam, torch, W, H);
    const Projected pm2 = project(torchCam, ctx.system.state(mars).position, ctx.system.def(mars).radius, W, H);
    const double tOff = meanLumaDisc(darkMars, pmars.x, pmars.y, pmars.radius * 0.7);
    const double tOn = meanLumaDisc(litMars, pm2.x, pm2.y, pm2.radius * 0.7);
    check("джерело 3 (прожектор): у вузькому конусі ліхтаря тіло світліше щонайменше втричі",
          tOn > 3.0 * tOff && tOn > 0.03, "з ліхтарем " + num(tOn, "%.3f") + ", без " + num(tOff, "%.4f"));

    cam = presetCamera(ctx, "earth", 100.0);
    RenderSettings tex = plain();
    tex.shadows = false;
    const Image textured = frame(ctx, cam, tex, W, H);
    tex.textures = false;
    const Image untextured = frame(ctx, cam, tex, W, H);
    auto saturation = [&](const Image& im) {
        double s = 0;
        int n = 0;
        for (int y = static_cast<int>(pe2.y - pe2.radius * 0.7); y < pe2.y + pe2.radius * 0.7; ++y) {
            for (int x = static_cast<int>(pe2.x - pe2.radius * 0.7); x < pe2.x + pe2.radius * 0.7; ++x) {
                const unsigned char* p = im.pixel(x, y);
                const int mx = std::max({p[0], p[1], p[2]}), mn = std::min({p[0], p[1], p[2]});
                if (mx > 40) {
                    s += static_cast<double>(mx - mn) / mx;
                    ++n;
                }
            }
        }
        return n > 0 ? s / n : 0.0;
    };
    const double satTex = saturation(textured), satFlat = saturation(untextured);
    check("текстури: з картою Земля кольорова (насиченість > 0,25), без неї — нейтрально сіра (< 0,08)", satTex > 0.25 && satFlat < 0.08,
          "насиченість " + num(satTex) + " проти " + num(satFlat));

    // Інтерфейс
    RenderSettings hudOn = plain();
    hudOn.hud.visible = true;
    hudOn.hud.selectedId = "earth";
    const Image withHud = frame(ctx, cam, hudOn, W, H);
    hudOn.hud.visible = false;
    const Image withoutHud = frame(ctx, cam, hudOn, W, H);
    long changedTop = 0, changedCenter = 0;
    for (int y = 0; y < 80; ++y) {
        for (int x = 0; x < 260; ++x) {
            if (std::abs(static_cast<int>(withHud.pixel(x, y)[0]) - static_cast<int>(withoutHud.pixel(x, y)[0])) > 20) ++changedTop;
        }
    }
    for (int y = H / 2 - 20; y < H / 2 + 20; ++y) {
        for (int x = W / 2 - 20; x < W / 2 + 20; ++x) {
            if (withHud.pixel(x, y)[0] != withoutHud.pixel(x, y)[0]) ++changedCenter;
        }
    }
    check("екранний інтерфейс: панель у лівому верхньому куті з'являється (≥ 300 змінених пікселів) і не зачіпає центр кадру",
          changedTop >= 300 && changedCenter == 0, num(static_cast<double>(changedTop), "%.0f") + " пікселів");

    Hud hud;
    std::string herr;
    bool sprites = hud.init(&herr);
    for (int i = 0; i < ctx.system.bodyCount() && sprites; ++i) sprites = sprites && hud.hasSprite("body_" + ctx.system.def(i).id);
    sprites = sprites && hud.hasSprite("body_ship") && hud.hasSprite("glyph_dot") && hud.hasSprite("help");
    for (char c = '0'; c <= '9'; ++c) sprites = sprites && hud.hasSprite(std::string("glyph_") + c);
    hud.shutdown();
    check("атлас інтерфейсу містить назви всіх тіл і корабля, цифри, довідку", sprites, herr);

    // Пропорції: коло залишається колом за різних співвідношень сторін
    cam = presetCamera(ctx, "earth", 100.0);
    RenderSettings circ = plain();
    circ.outline = true;
    circ.selected = earth;
    double ratios[2];
    const int sizes[2][2] = {{640, 360}, {300, 560}};
    for (int k = 0; k < 2; ++k) {
        const Image im = frame(ctx, cam, circ, sizes[k][0], sizes[k][1]);
        int x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1;
        for (int y = 0; y < im.height; ++y) {
            for (int x = 0; x < im.width; ++x) {
                if (isOrange(im.pixel(x, y))) {
                    x0 = std::min(x0, x);
                    x1 = std::max(x1, x);
                    y0 = std::min(y0, y);
                    y1 = std::max(y1, y);
                }
            }
        }
        ratios[k] = x1 > x0 && y1 > y0 ? static_cast<double>(x1 - x0) / (y1 - y0) : 0.0;
    }
    check("зміна розміру вікна: кола лишаються колами (відношення ширини до висоти контуру 1 ± 0,03) і в альбомній, і в портретній орієнтації",
          std::fabs(ratios[0] - 1.0) < 0.03 && std::fabs(ratios[1] - 1.0) < 0.03, num(ratios[0]) + " і " + num(ratios[1]));

    // Відтворюваність, стан OpenGL, швидкість
    cam = presetCamera(ctx, "inner", 40.0);
    RenderSettings full;
    const Image r1 = frame(ctx, cam, full, W, H, 4);
    const Image r2 = frame(ctx, cam, full, W, H, 4);
    check("відтворюваність: два кадри з однаковими параметрами збігаються байт у байт", !r1.empty() && r1.pixels == r2.pixels);

    GLint fbo = -1;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    GLboolean mask = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
    GLint stencilMask = 0;
    glGetIntegerv(GL_STENCIL_WRITEMASK, &stencilMask);
    const bool clean = glGetError() == GL_NO_ERROR && glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_CULL_FACE) &&
                       !glIsEnabled(GL_BLEND) && !glIsEnabled(GL_STENCIL_TEST) && mask == GL_TRUE && stencilMask == 0xFF;
    check("стан OpenGL після кадру: без помилок, глибина й відсікання ввімкнені, змішування й трафарет вимкнені, запис глибини дозволено",
          clean);

    Offscreen speed;
    speed.create(1280, 720, 4);
    RenderTarget rt;
    rt.framebuffer = speed.framebuffer();
    rt.width = 1280;
    rt.height = 720;
    RenderSettings fullSettings;
    fullSettings.selected = earth;
    ctx.renderer.render(ctx.system, cam, fullSettings, rt);
    glFinish();
    const auto t0 = std::chrono::steady_clock::now();
    const int frames = 30;
    for (int i = 0; i < frames; ++i) {
        ctx.renderer.render(ctx.system, cam, fullSettings, rt);
        glFinish();
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
    check("швидкодія: повний кадр 1280 × 720 (MSAA 4, тіні 6 граней, пояс 2600 тіл) відтворюється швидше за 50 мс", ms < 50.0,
          num(ms, "%.2f") + " мс на кадр");
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (!assets::locate()) {
        std::printf("Не знайдено каталог assets\n");
        return 2;
    }
    if (!glfwInit()) {
        std::printf("GLFW не ініціалізовано\n");
        return 2;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "selftest", nullptr, nullptr);
    if (window == nullptr) {
        std::printf("Не вдалося створити контекст OpenGL 3.3\n");
        return 2;
    }
    glfwMakeContextCurrent(window);
    std::string missing;
    const bool loaded = loadGlFunctions(reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress), &missing);
    std::printf("OpenGL: %s, %s\n", glGetString(GL_VERSION) ? reinterpret_cast<const char*>(glGetString(GL_VERSION)) : "?",
                glGetString(GL_RENDERER) ? reinterpret_cast<const char*>(glGetString(GL_RENDERER)) : "?");
    std::printf("Сцена: модель Сонячної системи (15 тіл, корабель, пояс астероїдів)\n\n");

    GLint major = 0, minor = 0, profile = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    check("контекст OpenGL ≥ 3.3, профіль core; усі потрібні функції завантажено", loaded && (major > 3 || (major == 3 && minor >= 3)) &&
              (profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0,
          "версія " + std::to_string(major) + "," + std::to_string(minor) + (missing.empty() ? "" : ", немає: " + missing));

    testMatrices();
    testCamera();
    testScene();
    testGeometry();
    testAssets();
    testShaders();
    testTextureOrientation();

    Ctx* ctx = new Ctx();
    std::string error;
    const bool ready = ctx->renderer.init(ctx->system, &error, 1024);
    check("рендерер ініціалізується: шейдери, 15 наборів текстур, моделі, кубічна карта тіней 1024² на грань", ready,
          ready ? "" : error);
    if (ready) {
        testPresets(*ctx);
        testRendering(*ctx);
    }
    ctx->renderer.shutdown();
    delete ctx;
    glfwDestroyWindow(window);
    glfwTerminate();

    std::printf("\nПідсумок: %d перевірок пройдено, %d не пройдено\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
