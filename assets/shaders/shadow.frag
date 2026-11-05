#version 330 core
// Прохід тіней, крок 3: у карту глибини записується лінійна відстань від джерела до точки,
// поділена на дальню межу (0…1). Так порівняння не залежить від перспективної нелінійності.
in vec3 fWorldPos;
in vec2 fUV;

uniform vec3 uLightPos;
uniform float uShadowFar;
uniform bool uAlphaTest;      // кільце Сатурна: прозорі місця тінь не відкидають
uniform sampler2D uAlphaMap;

void main() {
    // Береться згладжений рівень міпмапи: тонка структура кільця не дає «іскріння» тіні
    if (uAlphaTest && textureLod(uAlphaMap, fUV, 3.0).a < 0.22) {
        discard;
    }
    gl_FragDepth = length(fWorldPos - uLightPos) / uShadowFar;
}
