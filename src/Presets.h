#pragma once

// Готові ракурси камери: використовуються клавішею R / цифрами та автоматичними знімками
// для звіту (параметри --view і --time у командному рядку).

#include <string>
#include <vector>

#include "Camera.h"
#include "SolarSystem.h"

struct ViewPreset {
    std::string name;         // "overview", "earth", "solar-eclipse" …
    double days;              // момент часу за замовчуванням
    std::string selectedId;   // тіло, яке виділяється контуром (може бути порожнім)
};

const std::vector<ViewPreset>& viewPresets();
// Знаходить ракурс за ім'ям; повертає nullptr, якщо такого немає
const ViewPreset* findPreset(const std::string& name);

// Ставить камеру в ракурс для стану системи (матриці вже мають бути обчислені для потрібного часу)
void applyPreset(const ViewPreset& preset, const SolarSystem& system, Camera& camera);
