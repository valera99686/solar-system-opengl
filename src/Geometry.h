#pragma once

// Геометрія в оперативній пам'яті: процедурні фігури (сфера, кільце, куб, квадрат, коло)
// і завантаження моделей у форматі Wavefront OBJ. Розкладка вершини спільна для всіх
// шейдерів: позиція, нормаль, текстурні координати, дотичний вектор (для карт нормалей).

#include <filesystem>
#include <string>
#include <vector>

#include "Math.h"

struct Vertex {
    gfx::Vec3 position;
    gfx::Vec3 normal;
    gfx::Vec2 uv;
    gfx::Vec3 tangent;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;   // порожній — малювати масивами вершин
};

// Одинична сфера. u = довгота / 2π, v = (широта + 90°) / 180°; вісь обертання — y.
// Нормаль збігається з позицією, дотичний вектор — напрям зростання довготи.
MeshData makeSphere(int stacks, int slices);

// Плоске кільце в площині xOz. uv.x — радіальна координата (0 — внутрішній край, 1 — зовнішній),
// uv.y — кут / 2π; нормаль +y.
MeshData makeRing(float innerRadius, float outerRadius, int segments);

MeshData makeCube();                 // одиничний куб [−1; 1]³ (для неба)
MeshData makeQuad();                 // квадрат [−1; 1]² у площині xOy, нормаль +z
MeshData makeCircleLine(int segments);   // коло радіуса 1 у площині xOz (для GL_LINE_LOOP)

// Завантаження OBJ: v, vt, vn, f (многокутники розбиваються віялом на трикутники).
// Якщо нормалей у файлі немає, вони обчислюються; дотичні рахуються за текстурними координатами.
bool loadObj(const std::filesystem::path& file, MeshData& out, std::string* error = nullptr);
bool parseObj(const std::string& text, MeshData& out, std::string* error = nullptr);

void computeNormals(MeshData& mesh);
void computeTangents(MeshData& mesh);
