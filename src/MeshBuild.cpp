#include "MeshBuild.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

ofMesh buildTerrainMesh(const Heightfield& H, const TerrainWorld& W) {
    ofMesh m;
    m.setMode(OF_PRIMITIVE_TRIANGLES);

    const int w = H.width;
    const int h = H.height;
    if (w < 2 || h < 2) return m;

    // Compute min/max of raw heights
    float minH = std::numeric_limits<float>::max();
    float maxH = std::numeric_limits<float>::lowest();
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const float z = H.h(xx, yy);
            if (std::isfinite(z)) {
                minH = std::min(minH, z);
                maxH = std::max(maxH, z);
            }
        }
    }
    const float invRange = (maxH > minH) ? 1.0f / (maxH - minH) : 0.0f;

    // Ground spacing for fixed world footprint (X/Z ground, Y up)
    const float sx = (w > 1) ? (W.worldWidth  / static_cast<float>(w - 1)) : 0.0f;
    const float sz = (h > 1) ? (W.worldDepth  / static_cast<float>(h - 1)) : 0.0f;
    const float ox = W.center ? -0.5f * W.worldWidth : 0.0f;
    const float oz = W.center ? -0.5f * W.worldDepth : 0.0f;

    // Prepare containers
    m.clear();
    m.setMode(OF_PRIMITIVE_TRIANGLES);
    m.getVertices().reserve(static_cast<size_t>(w) * static_cast<size_t>(h));
    m.getNormals().resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    m.getColors().resize(static_cast<size_t>(w) * static_cast<size_t>(h));

    auto idx = [&](int x, int y) { return y * w + x; };

    // Vertices + colors (Y-up; normalized heights)
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const float Xg = ox + xx * sx;
            const float Zg = oz + yy * sz;
            const float hRaw = H.h(xx, yy);
            const float hN = (maxH > minH && std::isfinite(hRaw)) ? (hRaw - minH) * invRange : 0.0f;
            const float Yh = hN * W.heightScale; // final height on Y
            m.addVertex(glm::vec3(Xg, Yh, Zg));
            // per-vertex color ramp: green -> gray -> white
            ofFloatColor c;
            if (hN < 0.5f) {
                const float t = (hN) / 0.5f;
                const ofFloatColor low(0.22f, 0.42f, 0.28f, 1.0f); // deep green
                const ofFloatColor mid(0.60f, 0.60f, 0.60f, 1.0f); // mid gray
                c = low.getLerped(mid, t);
            } else {
                const float t = (hN - 0.5f) / 0.5f;
                const ofFloatColor mid(0.60f, 0.60f, 0.60f, 1.0f);
                const ofFloatColor high(0.95f, 0.95f, 0.95f, 1.0f); // near white
                c = mid.getLerped(high, t);
            }
            m.setColor(idx(xx, yy), c);
        }
    }

    // Indices (CCW)
    m.getIndices().reserve(static_cast<size_t>(w - 1) * static_cast<size_t>(h - 1) * 6);
    for (int yy = 0; yy < h - 1; ++yy) {
        for (int xx = 0; xx < w - 1; ++xx) {
            const int i0 = idx(xx,     yy);
            const int i1 = idx(xx + 1, yy);
            const int i2 = idx(xx,     yy + 1);
            const int i3 = idx(xx + 1, yy + 1);
            m.addIndex(i0); m.addIndex(i1); m.addIndex(i2);
            m.addIndex(i1); m.addIndex(i3); m.addIndex(i2);
        }
    }

    // Normals via finite differences (Y-up)
    auto& verts = m.getVertices();
    auto& norms = m.getNormals();
    auto vget = [&](int x, int y) { return verts[idx(x, y)]; };
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const int xm = std::max(0, xx - 1), xp = std::min(w - 1, xx + 1);
            const int ym = std::max(0, yy - 1), yp = std::min(h - 1, yy + 1);
            const glm::vec3 dx = vget(xp, yy) - vget(xm, yy);
            const glm::vec3 dz = vget(xx, yp) - vget(xx, ym);
            glm::vec3 n = glm::cross(dz, dx); // Y-up with CCW winding
            const float len = glm::length(n);
            if (len > 0.0f) n /= len; else n = glm::vec3(0,1,0);
            norms[idx(xx, yy)] = n;
        }
    }

    return m;
}
