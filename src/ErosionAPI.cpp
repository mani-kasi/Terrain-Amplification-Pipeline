#include "ErosionAPI.h"
#include "Erosion.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    // degrees -> radians without relying on M_PI
    constexpr float kDeg2Rad = 0.01745329251994329577f; // pi/180
    constexpr float kEpsA    = 1e-6f;

    // Compute slope magnitude |∇h| per cell (grid units)
    void computeSlopeMag(const Heightfield& H, std::vector<float>& out) {
        const int w = H.width;
        const int h = H.height;
        if (w <= 0 || h <= 0) {
            out.clear();
            return;
        }
        out.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0.0f);
        auto idx = [w](int x, int y) { return y * w + x; };

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int xm = std::max(0, x - 1);
                const int xp = std::min(w - 1, x + 1);
                const int ym = std::max(0, y - 1);
                const int yp = std::min(h - 1, y + 1);

                const float dzdx = H.h(xp, y) - H.h(xm, y);
                const float dzdy = H.h(x, yp) - H.h(x, ym);

                const float gx = dzdx * 0.5f;
                const float gy = dzdy * 0.5f;
                out[static_cast<std::size_t>(idx(x, y))] = std::sqrt(gx * gx + gy * gy);
            }
        }
    }
}

// dh -= Kf * A^p * S^q, hardness-scaled, with per-step clamping
void runFluvialPass(Heightfield& H, int iters, float Kf, float p, float q) {
    const int w = H.width;
    const int h = H.height;
    if (w <= 0 || h <= 0 || iters <= 0 || Kf == 0.0f) {
        return;
    }

    const int n = w * h;

    const float maxErodePerIter = 0.4f; // clamp in normalized height units
    std::vector<float> S;
    S.reserve(static_cast<std::size_t>(n));

    for (int it = 0; it < iters; ++it) {
        // Recompute slope and drainage each iteration so channels can keep sharpening
        computeSlopeMag(H, S);
        if (static_cast<int>(S.size()) != n) {
            return;
        }

        // Drainage area A for current surface
        computeDrainage(H); // fills H.drainage

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int i = y * w + x;
                const float A = std::max(H.drainage[static_cast<std::size_t>(i)], 0.0f) + kEpsA;
                const float Smag = std::max(S[static_cast<std::size_t>(i)], 0.0f);

                float cap = Kf * std::pow(A, p) * std::pow(Smag, q);
                const float hard = ofClamp(H.hard(x, y), 0.0f, 1.0f);
                const float hardnessFactor = 0.2f + 0.8f * (1.0f - hard);
                cap *= hardnessFactor;
                cap = ofClamp(cap, -maxErodePerIter, maxErodePerIter);

                H.elevation[static_cast<std::size_t>(i)] -= cap;
                if (!std::isfinite(H.elevation[static_cast<std::size_t>(i)])) {
                    H.elevation[static_cast<std::size_t>(i)] = 0.0f;
                }
            }
        }
        // Optional: diffusion/smoothing could be added here if needed.
    }
}

void runThermalPass(Heightfield& H, int iters, float Kt, float talusDeg) {
    ThermalParams params;
    params.c = Kt;
    // talusDeg is the angle of repose in degrees; threshold is tan(theta)
    const float talusRad = talusDeg * kDeg2Rad;
    const float talusTan = static_cast<float>(std::tan(talusRad));
    params.talusAngle = talusTan;

    const int steps = std::max(0, iters);
    for (int i = 0; i < steps; ++i) {
        applyThermalErosion(H, params);
    }
}

// Light deposition pass: smooths gentle slopes (widens valley floors)
void runDepositionPass(Heightfield& H, int iters, float Kd, float slopeCutoff) {
    const int w = H.width;
    const int h = H.height;
    if (w <= 0 || h <= 0 || iters <= 0 || Kd == 0.0f) {
        return;
    }

    // Use a flat array for slope magnitude, reusing the helper
    std::vector<float> S;
    computeSlopeMag(H, S);
    if (static_cast<int>(S.size()) != w * h) {
        return;
    }

    const float maxMove = 0.25f; // clamp per-iter movement (normalized units)
    auto idx = [w](int x, int y) { return y * w + x; };

    for (int it = 0; it < iters; ++it) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int i = idx(x, y);
                if (S[static_cast<std::size_t>(i)] >= slopeCutoff) {
                    continue; // only deposit on gentle areas
                }

                const int xm = std::max(0, x - 1);
                const int xp = std::min(w - 1, x + 1);
                const int ym = std::max(0, y - 1);
                const int yp = std::min(h - 1, y + 1);

                const float mean4 =
                    0.25f * (H.h(xm, y) + H.h(xp, y) +
                             H.h(x, ym) + H.h(x, yp));

                float delta = Kd * (mean4 - H.h(x, y)); // pull toward neighbor mean
                delta = ofClamp(delta, -maxMove, maxMove);       // safety
                H.h(x, y) += delta;
            }
        }
        // refresh slope after each small pass so the mask stays meaningful
        computeSlopeMag(H, S);
    }
}
