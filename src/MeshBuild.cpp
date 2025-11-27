#include "MeshBuild.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

ofMesh buildTerrainMesh(const Heightfield& H, const TerrainWorld& W) {
    ofMesh m;
    m.setMode(OF_PRIMITIVE_TRIANGLES);

    const int w = H.width;
    const int h = H.height;
    if (w < 2 || h < 2) return m;

    // Compute min/max of raw heights for normalization into world Y
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
    m.getVertices().reserve(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    m.getNormals().resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    m.getColors().resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));

    auto idx = [&](int x, int y) { return y * w + x; };

    // Vertices only (Y-up; normalized heights for world Y placement)
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const float Xg = ox + xx * sx;
            const float Zg = oz + yy * sz;
            const float hRaw = H.h(xx, yy);
            const float hN = (maxH > minH && std::isfinite(hRaw)) ? (hRaw - minH) * invRange : 0.0f;
            const float Yh = hN * W.heightScale; // final height on Y
            m.addVertex(glm::vec3(Xg, Yh, Zg));
        }
    }

    // Indices (CCW)
    m.getIndices().reserve(static_cast<std::size_t>(w - 1) * static_cast<std::size_t>(h - 1) * 6);
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
            const glm::vec3 dxv = vget(xp, yy) - vget(xm, yy);
            const glm::vec3 dzv = vget(xx, yp) - vget(xx, ym);
            glm::vec3 n = glm::cross(dzv, dxv); // Y-up with CCW winding
            const float len = glm::length(n);
            if (len > 0.0f) n /= len; else n = glm::vec3(0, 1, 0);
            norms[idx(xx, yy)] = n;
        }
    }

    return m;
}

void colorByHeight(ofMesh& mesh, const Heightfield& H) {
    const int w = H.width;
    const int h = H.height;
    if (w <= 0 || h <= 0) return;

    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    auto& colors = mesh.getColors();
    if (colors.size() != n) {
        colors.resize(n);
    }

    float minH = std::numeric_limits<float>::max();
    float maxH = std::numeric_limits<float>::lowest();
    for (float e : H.elevation) {
        if (std::isfinite(e)) {
            minH = std::min(minH, e);
            maxH = std::max(maxH, e);
        }
    }
    const float invRange = (maxH > minH) ? 1.0f / (maxH - minH) : 0.0f;

    auto idx = [&](int x, int y) { return y * w + x; };
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const float hRaw = H.h(xx, yy);
            const float hN = (maxH > minH && std::isfinite(hRaw)) ? (hRaw - minH) * invRange : 0.0f;

            ofFloatColor c;
            if (hN < 0.5f) {
                const float t = hN / 0.5f;
                const ofFloatColor low(0.22f, 0.42f, 0.28f, 1.0f); // deep green
                const ofFloatColor mid(0.60f, 0.60f, 0.60f, 1.0f); // mid gray
                c = low.getLerped(mid, t);
            } else {
                const float t = (hN - 0.5f) / 0.5f;
                const ofFloatColor mid(0.60f, 0.60f, 0.60f, 1.0f);
                const ofFloatColor high(0.95f, 0.95f, 0.95f, 1.0f); // near white
                c = mid.getLerped(high, t);
            }
            mesh.setColor(idx(xx, yy), c);
        }
    }
}

void colorByLogDrainage(ofMesh& mesh, const Heightfield& H, const Heightfield& drainageField) {
    const int w = H.width;
    const int h = H.height;
    if (w <= 0 || h <= 0) return;
    if (drainageField.width != w || drainageField.height != h) {
        return;
    }

    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    if (drainageField.drainage.size() < n) {
        return;
    }

    auto& colors = mesh.getColors();
    if (colors.size() != n) {
        colors.resize(n);
    }

    float minL = std::numeric_limits<float>::max();
    float maxL = std::numeric_limits<float>::lowest();
    for (std::size_t i = 0; i < n; ++i) {
        const float A = drainageField.drainage[i];
        if (!std::isfinite(A)) continue;
        const float L = std::log10(A + 1e-6f);
        minL = std::min(minL, L);
        maxL = std::max(maxL, L);
    }
    const float invRange = (maxL > minL) ? 1.0f / (maxL - minL) : 0.0f;

    auto idx = [&](int x, int y) { return y * w + x; };
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const std::size_t i = static_cast<std::size_t>(idx(xx, yy));
            const float A = drainageField.drainage[i];
            float L = std::log10(A + 1e-6f);
            float t = (maxL > minL && std::isfinite(L)) ? (L - minL) * invRange : 0.0f;
            t = ofClamp(t, 0.0f, 1.0f);

            const ofFloatColor c0(0.05f, 0.10f, 0.30f, 1.0f); // dark blue
            const ofFloatColor c1(0.10f, 0.70f, 0.80f, 1.0f); // cyan
            const ofFloatColor c2(0.95f, 0.97f, 1.00f, 1.0f); // near white

            ofFloatColor c;
            if (t < 0.5f) {
                const float k = t / 0.5f;
                c = c0.getLerped(c1, k);
            } else {
                const float k = (t - 0.5f) / 0.5f;
                c = c1.getLerped(c2, k);
            }
            mesh.setColor(static_cast<int>(i), c);
        }
    }
}

void colorBySlope(ofMesh& mesh, const Heightfield& H, float dx, float dy) {
    const int w = H.width;
    const int h = H.height;
    if (w <= 0 || h <= 0 || dx <= 0.0f || dy <= 0.0f) return;

    const std::size_t n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    auto& colors = mesh.getColors();
    if (colors.size() != n) {
        colors.resize(n);
    }

    std::vector<float> slopes(n, 0.0f);
    float minS = std::numeric_limits<float>::max();
    float maxS = std::numeric_limits<float>::lowest();

    auto idx = [&](int x, int y) { return y * w + x; };

    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            const int xm = std::max(0, xx - 1), xp = std::min(w - 1, xx + 1);
            const int ym = std::max(0, yy - 1), yp = std::min(h - 1, yy + 1);

            const float dzdx = H.h(xp, yy) - H.h(xm, yy);
            const float dzdy = H.h(xx, yp) - H.h(xx, ym);

            const float ddx = (xp != xm) ? (dx * static_cast<float>(xp - xm)) : dx;
            const float ddy = (yp != ym) ? (dy * static_cast<float>(yp - ym)) : dy;

            float gx = (ddx > 0.0f) ? dzdx / ddx : 0.0f;
            float gy = (ddy > 0.0f) ? dzdy / ddy : 0.0f;

            const float s = std::sqrt(gx * gx + gy * gy);
            const std::size_t i = static_cast<std::size_t>(idx(xx, yy));
            slopes[i] = s;
            if (std::isfinite(s)) {
                minS = std::min(minS, s);
                maxS = std::max(maxS, s);
            }
        }
    }

    const float invRange = (maxS > minS) ? 1.0f / (maxS - minS) : 0.0f;
    const ofFloatColor low(0.15f, 0.15f, 0.15f, 1.0f);
    const ofFloatColor high(0.95f, 0.95f, 0.95f, 1.0f);

    for (std::size_t i = 0; i < n; ++i) {
        float s = slopes[i];
        float t = (maxS > minS && std::isfinite(s)) ? (s - minS) * invRange : 0.0f;
        t = ofClamp(t, 0.0f, 1.0f);
        mesh.setColor(static_cast<int>(i), low.getLerped(high, t));
    }
}
