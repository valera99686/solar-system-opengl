#include "Geometry.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <tuple>

#include "Assets.h"

using gfx::Vec2;
using gfx::Vec3;

MeshData makeSphere(int stacks, int slices) {
    MeshData m;
    m.vertices.reserve(static_cast<size_t>(stacks + 1) * (slices + 1));
    for (int i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / stacks;
        const float lat = (v - 0.5f) * gfx::kPi;   // −90° … +90°
        for (int j = 0; j <= slices; ++j) {
            const float u = static_cast<float>(j) / slices;
            const float lon = u * 2.0f * gfx::kPi;
            Vertex vert;
            vert.position = {std::cos(lat) * std::cos(lon), std::sin(lat), -std::cos(lat) * std::sin(lon)};
            vert.normal = vert.position;
            vert.uv = {u, v};
            vert.tangent = {-std::sin(lon), 0.0f, -std::cos(lon)};
            m.vertices.push_back(vert);
        }
    }
    const unsigned stride = static_cast<unsigned>(slices + 1);
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const unsigned a = static_cast<unsigned>(i) * stride + static_cast<unsigned>(j);
            const unsigned b = a + 1;
            const unsigned c = a + stride;
            const unsigned d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, d, a, d, c});   // проти годинникової стрілки ззовні
        }
    }
    return m;
}

MeshData makeRing(float innerRadius, float outerRadius, int segments) {
    MeshData m;
    for (int i = 0; i <= segments; ++i) {
        const float u = static_cast<float>(i) / segments;
        const float a = u * 2.0f * gfx::kPi;
        const Vec3 dir(std::cos(a), 0.0f, std::sin(a));
        Vertex inner;
        inner.position = dir * innerRadius;
        inner.normal = {0.0f, 1.0f, 0.0f};
        inner.uv = {0.0f, u};
        inner.tangent = {-std::sin(a), 0.0f, std::cos(a)};
        Vertex outer = inner;
        outer.position = dir * outerRadius;
        outer.uv = {1.0f, u};
        m.vertices.push_back(inner);
        m.vertices.push_back(outer);
    }
    for (int i = 0; i < segments; ++i) {
        const unsigned a = static_cast<unsigned>(i) * 2;   // внутрішня i, зовнішня a + 1
        const unsigned b = a + 1;
        const unsigned c = a + 2;
        const unsigned d = a + 3;
        m.indices.insert(m.indices.end(), {a, d, b, a, c, d});   // проти годинникової стрілки зверху (+y)
    }
    return m;
}

MeshData makeCube() {
    MeshData m;
    for (int i = 0; i < 8; ++i) {
        Vertex v;
        v.position = {(i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f};
        v.normal = gfx::normalize(v.position);
        m.vertices.push_back(v);
    }
    const unsigned faces[6][4] = {{1, 3, 7, 5}, {0, 4, 6, 2}, {2, 6, 7, 3}, {0, 1, 5, 4}, {4, 5, 7, 6}, {0, 2, 3, 1}};
    for (const auto& f : faces) {
        m.indices.insert(m.indices.end(), {f[0], f[1], f[2], f[0], f[2], f[3]});
    }
    return m;
}

MeshData makeQuad() {
    MeshData m;
    const float xs[4] = {-1, 1, 1, -1};
    const float ys[4] = {-1, -1, 1, 1};
    for (int i = 0; i < 4; ++i) {
        Vertex v;
        v.position = {xs[i], ys[i], 0.0f};
        v.normal = {0.0f, 0.0f, 1.0f};
        v.uv = {xs[i] * 0.5f + 0.5f, ys[i] * 0.5f + 0.5f};
        v.tangent = {1.0f, 0.0f, 0.0f};
        m.vertices.push_back(v);
    }
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

MeshData makeCircleLine(int segments) {
    MeshData m;
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0f * gfx::kPi * static_cast<float>(i) / segments;
        Vertex v;
        v.position = {std::cos(a), 0.0f, std::sin(a)};
        v.normal = {0.0f, 1.0f, 0.0f};
        v.tangent = {-std::sin(a), 0.0f, std::cos(a)};
        m.vertices.push_back(v);
    }
    return m;
}

void computeNormals(MeshData& mesh) {
    for (Vertex& v : mesh.vertices) {
        v.normal = Vec3();
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        Vertex& a = mesh.vertices[mesh.indices[i]];
        Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 n = gfx::cross(b.position - a.position, c.position - a.position);   // довжина ∝ площі
        a.normal += n;
        b.normal += n;
        c.normal += n;
    }
    for (Vertex& v : mesh.vertices) {
        v.normal = gfx::normalize(v.normal);
    }
}

void computeTangents(MeshData& mesh) {
    std::vector<Vec3> tan(mesh.vertices.size());
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const Vertex& a = mesh.vertices[mesh.indices[i]];
        const Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 e1 = b.position - a.position;
        const Vec3 e2 = c.position - a.position;
        const float du1 = b.uv.x - a.uv.x, dv1 = b.uv.y - a.uv.y;
        const float du2 = c.uv.x - a.uv.x, dv2 = c.uv.y - a.uv.y;
        const float det = du1 * dv2 - du2 * dv1;
        if (std::fabs(det) < 1e-12f) {
            continue;
        }
        const Vec3 t = (e1 * dv2 - e2 * dv1) * (1.0f / det);
        tan[mesh.indices[i]] += t;
        tan[mesh.indices[i + 1]] += t;
        tan[mesh.indices[i + 2]] += t;
    }
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        Vertex& v = mesh.vertices[i];
        Vec3 t = tan[i] - v.normal * gfx::dot(v.normal, tan[i]);   // ортогоналізація Грама–Шмідта
        if (gfx::length(t) < 1e-8f) {
            const Vec3 axis = std::fabs(v.normal.y) < 0.9f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
            t = gfx::cross(axis, v.normal);
        }
        v.tangent = gfx::normalize(t);
    }
}

// ---------------------------------------------------------------------------
// OBJ
// ---------------------------------------------------------------------------
bool parseObj(const std::string& text, MeshData& out, std::string* error) {
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
    std::vector<Vec3> normals;
    std::map<std::tuple<int, int, int>, unsigned> cache;
    bool anyNormal = false;

    out.vertices.clear();
    out.indices.clear();

    std::istringstream stream(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(stream, line)) {
        ++lineNo;
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "v") {
            Vec3 p;
            ls >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (tag == "vt") {
            Vec2 t;
            ls >> t.x >> t.y;
            uvs.push_back(t);
        } else if (tag == "vn") {
            Vec3 n;
            ls >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (tag == "f") {
            std::vector<unsigned> face;
            std::string token;
            while (ls >> token) {
                int idx[3] = {0, 0, 0};   // v, vt, vn; 0 — відсутній
                int part = 0;
                size_t start = 0;
                for (size_t i = 0; i <= token.size() && part < 3; ++i) {
                    if (i == token.size() || token[i] == '/') {
                        if (i > start) {
                            idx[part] = std::atoi(token.substr(start, i - start).c_str());
                        }
                        ++part;
                        start = i + 1;
                    }
                }
                // Від'ємні індекси рахуються від кінця списку
                const int counts[3] = {static_cast<int>(positions.size()), static_cast<int>(uvs.size()),
                                       static_cast<int>(normals.size())};
                for (int k = 0; k < 3; ++k) {
                    if (idx[k] < 0) {
                        idx[k] += counts[k] + 1;
                    }
                }
                if (idx[0] < 1 || idx[0] > counts[0] || idx[1] > counts[1] || idx[2] > counts[2]) {
                    if (error != nullptr) {
                        *error = "рядок " + std::to_string(lineNo) + ": індекс поза межами";
                    }
                    return false;
                }
                const auto key = std::make_tuple(idx[0], idx[1], idx[2]);
                auto it = cache.find(key);
                if (it == cache.end()) {
                    Vertex v;
                    v.position = positions[static_cast<size_t>(idx[0] - 1)];
                    if (idx[1] > 0) {
                        v.uv = uvs[static_cast<size_t>(idx[1] - 1)];
                    }
                    if (idx[2] > 0) {
                        v.normal = gfx::normalize(normals[static_cast<size_t>(idx[2] - 1)]);
                        anyNormal = true;
                    }
                    it = cache.emplace(key, static_cast<unsigned>(out.vertices.size())).first;
                    out.vertices.push_back(v);
                }
                face.push_back(it->second);
            }
            if (face.size() < 3) {
                if (error != nullptr) {
                    *error = "рядок " + std::to_string(lineNo) + ": грань має менше трьох вершин";
                }
                return false;
            }
            for (size_t k = 1; k + 1 < face.size(); ++k) {   // віяло трикутників
                out.indices.insert(out.indices.end(), {face[0], face[k], face[k + 1]});
            }
        }
    }
    if (out.indices.empty()) {
        if (error != nullptr) {
            *error = "у файлі немає граней";
        }
        return false;
    }
    if (!anyNormal) {
        computeNormals(out);
    }
    computeTangents(out);
    return true;
}

bool loadObj(const std::filesystem::path& file, MeshData& out, std::string* error) {
    std::string text;
    if (!assets::readText(file, text)) {
        if (error != nullptr) {
            *error = "не вдалося відкрити файл " + file.u8string();
        }
        return false;
    }
    return parseObj(text, out, error);
}
