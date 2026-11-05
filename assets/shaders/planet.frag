#version 330 core
// Освітлення Блінна–Фонга від трьох джерел:
//   1) точкове Сонце з ослабленням і м'якими тінями (кубічна карта глибини);
//   2) напрямлене слабке «далеке світило»;
//   3) прожектор («ліхтар» камери).
// Кольорові текстури надходять у sRGB і перетворюються у лінійний простір апаратно;
// наприкінці результат повертається в sRGB (γ = 2,2).
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in mat3 vTBN;

out vec4 fragColor;

uniform sampler2D uDiffuse;          // 0
uniform sampler2D uSpecularMap;      // 1
uniform sampler2D uNormalMap;        // 2
uniform samplerCube uShadowMap;      // 3: відстані до найближчої поверхні, поділені на uShadowFar
uniform samplerCube uEnvironment;    // 4: небо (відбиття на кораблі)

uniform bool uTexturesOn;
uniform bool uHasSpecularMap;
uniform bool uHasNormalMap;
uniform bool uUseAlpha;              // кільце: прозорість із текстури
uniform bool uTwoSided;              // кільце видно з обох боків
uniform float uShininess;
uniform float uSpecularStrength;
uniform float uReflectivity;
uniform vec3 uAtmosphereColor;
uniform float uAtmosphereStrength;

uniform vec3 uCameraPos;
uniform vec3 uAmbient;

uniform bool uSunOn;
uniform vec3 uSunPos;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform float uSunAttenuation;

uniform vec3 uStarDir;               // напрям НА далеке світило
uniform vec3 uStarColor;

uniform bool uTorchOn;
uniform vec3 uTorchPos;
uniform vec3 uTorchDir;              // напрям променя
uniform vec3 uTorchColor;
uniform float uTorchInner;           // косинус внутрішнього кута
uniform float uTorchOuter;           // косинус зовнішнього кута

uniform bool uShadowsOn;
uniform float uShadowFar;
uniform float uShadowBiasExtra;      // додатковий зсув для кільця (воно майже паралельне променям)

const vec3 kDisk[20] = vec3[](
    vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
    vec3(1, 1, -1), vec3(1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
    vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(1, 0, 1), vec3(-1, 0, 1), vec3(1, 0, -1), vec3(-1, 0, -1),
    vec3(0, 1, 1), vec3(0, -1, 1), vec3(0, -1, -1), vec3(0, 1, -1));

// Частка променів, перекритих іншими тілами (0 — освітлено, 1 — повна тінь).
// 20 вибірок навколо напряму дають м'яку межу (PCF).
float sunShadow(vec3 worldPos, vec3 n, vec3 l) {
    if (!uShadowsOn) {
        return 0.0;
    }
    vec3 fromLight = worldPos - uSunPos;
    float current = length(fromLight);
    float cosT = max(dot(n, l), 0.0);
    float bias = (0.03 + current * 0.003) * (1.0 + 2.0 * (1.0 - cosT)) + uShadowBiasExtra;
    float radius = 0.025 + current * 0.0012;
    float shadow = 0.0;
    for (int i = 0; i < 20; ++i) {
        float closest = texture(uShadowMap, fromLight + kDisk[i] * radius).r * uShadowFar;
        shadow += (current - bias > closest) ? 1.0 : 0.0;
    }
    return shadow / 20.0;
}

vec3 blinnPhong(vec3 n, vec3 v, vec3 albedo, float specMask, vec3 l, vec3 radiance) {
    float diff = max(dot(n, l), 0.0);
    vec3 h = normalize(l + v);
    float spec = diff > 0.0 ? pow(max(dot(n, h), 0.0), uShininess) : 0.0;
    return (albedo * diff + vec3(spec * specMask * uSpecularStrength)) * radiance;
}

void main() {
    vec4 tex = uTexturesOn ? texture(uDiffuse, vUV) : vec4(0.7, 0.7, 0.7, 1.0);
    if (uUseAlpha && tex.a < 0.02) {
        discard;
    }
    vec3 albedo = tex.rgb;
    float specMask = uHasSpecularMap ? texture(uSpecularMap, vUV).r : 1.0;

    vec3 ng = normalize(vNormal);
    vec3 n = ng;
    if (uHasNormalMap) {
        vec3 t = texture(uNormalMap, vUV).xyz * 2.0 - 1.0;
        n = normalize(vTBN * t);
    }
    vec3 v = normalize(uCameraPos - vWorldPos);
    if (uTwoSided && dot(ng, v) < 0.0) {
        ng = -ng;
        n = -n;
    }

    vec3 color = albedo * uAmbient;

    if (uSunOn) {
        vec3 toSun = uSunPos - vWorldPos;
        float d = length(toSun);
        vec3 l = toSun / d;
        float att = 1.0 / (1.0 + uSunAttenuation * d * d);
        float lit = 1.0 - sunShadow(vWorldPos, ng, l);
        color += blinnPhong(n, v, albedo, specMask, l, uSunColor * uSunIntensity * att) * lit;
    }

    color += blinnPhong(n, v, albedo, specMask, normalize(uStarDir), uStarColor);

    if (uTorchOn) {
        vec3 toLight = uTorchPos - vWorldPos;
        float d = length(toLight);
        vec3 l = toLight / d;
        float theta = dot(-l, normalize(uTorchDir));
        float cone = clamp((theta - uTorchOuter) / (uTorchInner - uTorchOuter), 0.0, 1.0);
        float att = 1.0 / (1.0 + 0.03 * d + 0.0015 * d * d);
        color += blinnPhong(n, v, albedo, specMask, l, uTorchColor * cone * att);
    }

    if (uReflectivity > 0.0) {
        vec3 r = reflect(-v, n);
        float fresnel = 0.3 + 0.7 * pow(1.0 - max(dot(n, v), 0.0), 3.0);
        color = mix(color, texture(uEnvironment, r).rgb, uReflectivity * fresnel);
    }

    if (uAtmosphereStrength > 0.0) {
        float rim = pow(1.0 - max(dot(ng, v), 0.0), 3.0);
        float dayside = uSunOn ? clamp(dot(ng, normalize(uSunPos - vWorldPos)) + 0.25, 0.0, 1.0) : 0.0;
        color += uAtmosphereColor * rim * uAtmosphereStrength * dayside;
    }

    fragColor = vec4(pow(color, vec3(1.0 / 2.2)), uUseAlpha ? tex.a : 1.0);
}
