#include "Erosion.h"

#include "ofMain.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace {
    struct NeighborOffset {
        int dx;
        int dy;
        float dist;
    };

    const NeighborOffset kNeighbors[8] = {
        {-1, -1, static_cast<float>(std::sqrt(2.0f))},
        { 0, -1, 1.0f},
        { 1, -1, static_cast<float>(std::sqrt(2.0f))},
        {-1,  0, 1.0f},
        { 1,  0, 1.0f},
        {-1,  1, static_cast<float>(std::sqrt(2.0f))},
        { 0,  1, 1.0f},
        { 1,  1, static_cast<float>(std::sqrt(2.0f))}
    };
}

// Single-flow (D8) steepest-descent routing; simplified vs. paper's multi-flow
void computeDrainage(Heightfield& hf) {
    const int w = hf.width;
    const int h = hf.height;
    if (w <= 0 || h <= 0) {
        return;
    }

    const int n = w * h;
    std::vector<int> flowTarget(n, -1);

    auto idx = [w](int x, int y) {
        return y * w + x;
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = idx(x, y);
            const float zHere = hf.h(x, y);

            float bestSlope = 0.0f;
            int bestTarget = -1;

            for (const auto& nb : kNeighbors) {
                const int nx = x + nb.dx;
                const int ny = y + nb.dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                    continue;
                }

                const int j = idx(nx, ny);
                const float zNeighbor = hf.h(nx, ny);
                if (zHere > zNeighbor) {
                    const float slope = (zHere - zNeighbor) / nb.dist;
                    if (slope > bestSlope) {
                        bestSlope = slope;
                        bestTarget = j;
                    }
                }
            }

            flowTarget[i] = bestTarget;
        }
    }

    std::fill(hf.drainage.begin(), hf.drainage.end(), 1.0f);

    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&hf](int a, int b) {
                  return hf.elevation[a] > hf.elevation[b];
              });

    for (int cell : order) {
        const int target = flowTarget[cell];
        if (target >= 0) {
            hf.drainage[target] += hf.drainage[cell];
        }
    }
}

void applyFluvialErosion(Heightfield& hf, const FluvialParams& params) {
    const int w = hf.width;
    const int h = hf.height;
    if (w <= 0 || h <= 0) {
        return;
    }

    const int n = w * h;

    auto idx = [w](int x, int y) {
        return y * w + x;
    };

    std::vector<float> deltaHeight(n, 0.0f);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = idx(x, y);
            const float zHere = hf.h(x, y);

            float bestSlope = 0.0f;

            for (const auto& nb : kNeighbors) {
                const int nx = x + nb.dx;
                const int ny = y + nb.dy;
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                    continue;
                }

                const float zNeighbor = hf.h(nx, ny);
                if (zHere > zNeighbor) {
                    const float slope = (zHere - zNeighbor) / nb.dist;
                    if (slope > bestSlope) {
                        bestSlope = slope;
                    }
                }
            }

            if (bestSlope <= 0.0f) {
                continue;
            }

            const float slope = std::max(bestSlope, params.minSlope);
            const float drainage = hf.drain(x, y);
            const float streamPower = drainage * slope;
            float de = params.k * streamPower * params.dt * params.intensity;
            if (de <= 0.0f) {
                continue;
            }

            const float hardness = ofClamp(hf.hard(x, y), 0.0f, 1.0f);
            const float softness = 1.0f - hardness;
            const float hardnessFactor = 0.2f + 0.8f * softness;

            deltaHeight[i] = de * hardnessFactor;
        }
    }

    for (int i = 0; i < n; ++i) {
        if (deltaHeight[i] > 0.0f) {
            hf.elevation[i] -= deltaHeight[i];
            if (hf.elevation[i] < 0.0f) {
                hf.elevation[i] = 0.0f;
            }
        }
    }
}

void applyThermalErosion(Heightfield& hf, const ThermalParams& params) {
    const int w = hf.width;
    const int h = hf.height;
    if (w <= 0 || h <= 0) {
        return;
    }

    const int n = w * h;

    auto idx = [w](int x, int y) {
        return y * w + x;
    };

    const int dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    const int dy[8] = { -1,-1,-1,  0, 0,  1, 1, 1 };

    std::vector<float> deltaHeight(n, 0.0f);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = idx(x, y);
            const float zHere = hf.h(x, y);

            const float hardHere = ofClamp(hf.hard(x, y), 0.0f, 1.0f);
            const float hardnessFactor = 0.2f + 0.8f * (1.0f - hardHere);

            for (int k = 0; k < 8; ++k) {
                const int nx = x + dx[k];
                const int ny = y + dy[k];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                    continue;
                }

                const int j = idx(nx, ny);
                const float zNeighbor = hf.h(nx, ny);
                const float dh = zHere - zNeighbor;

                if (dh > params.talusAngle) {
                    float excess = dh - params.talusAngle;
                    float m = params.c * excess * hardnessFactor;

                    if (m > 0.0f) {
                        deltaHeight[i] -= m;
                        deltaHeight[j] += m;
                    }
                }
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        hf.elevation[i] += deltaHeight[i];
        if (hf.elevation[i] < 0.0f) {
            hf.elevation[i] = 0.0f;
        }
    }
}
