#include "Hydrology.h"

#include <queue>
#include <vector>
#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>

struct PFNode {
    int x;
    int y;
    float z;
};

struct PFCompare {
    bool operator()(const PFNode& a, const PFNode& b) const {
        return a.z > b.z; // min-heap
    }
};

static inline int pfIdx(int x, int y, int w) {
    return y * w + x;
}

void priorityFloodFill(Heightfield& H, float epsRaise) {
    const int W = H.width;
    const int Hh = H.height;
    if (W <= 1 || Hh <= 1) return;

    std::priority_queue<PFNode, std::vector<PFNode>, PFCompare> pq;
    std::vector<std::uint8_t> vis(static_cast<std::size_t>(W) * static_cast<std::size_t>(Hh), 0);

    auto pushCell = [&](int x, int y) {
        const int i = pfIdx(x, y, W);
        pq.push(PFNode{x, y, H.elevation[static_cast<std::size_t>(i)]});
        vis[static_cast<std::size_t>(i)] = 1;
    };

    // Seed with border cells
    for (int x = 0; x < W; ++x) {
        pushCell(x, 0);
        pushCell(x, Hh - 1);
    }
    for (int y = 1; y < Hh - 1; ++y) {
        pushCell(0, y);
        pushCell(W - 1, y);
    }

    // 4-neighbour expansion
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (!pq.empty()) {
        const PFNode n = pq.top();
        pq.pop();

        for (int k = 0; k < 4; ++k) {
            const int nx = n.x + dx[k];
            const int ny = n.y + dy[k];
            if (nx < 0 || ny < 0 || nx >= W || ny >= Hh) continue;

            const int j = pfIdx(nx, ny, W);
            if (vis[static_cast<std::size_t>(j)]) continue;

            const float z = H.elevation[static_cast<std::size_t>(j)];
            float zNew = std::max(z, n.z + epsRaise); // minimal raise to ensure monotonic flow
            if (!std::isfinite(zNew)) {
                zNew = n.z;
            }

            H.elevation[static_cast<std::size_t>(j)] = zNew;
            vis[static_cast<std::size_t>(j)] = 1;
            pq.push(PFNode{nx, ny, zNew});
        }
    }
}

namespace {
    const int kDx8[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    const int kDy8[8] = { -1,-1,-1,  0, 0,  1, 1, 1 };

    struct BasinStats {
        std::vector<int> cells;
        float maxDelta = 0.0f;
        float minHeight = std::numeric_limits<float>::infinity();
    };

    // Simple Dijkstra to find a low-cost path between two cells (8-neighbour)
    std::vector<int> findBreachPath(const Heightfield& H, int srcIdx, int dstIdx) {
        const int W = H.width;
        const int Hh = H.height;
        const int N = W * Hh;
        if (srcIdx < 0 || srcIdx >= N || dstIdx < 0 || dstIdx >= N || N == 0) {
            return {};
        }

        const float spillH = H.elevation[static_cast<std::size_t>(dstIdx)];
        const float alpha = 0.5f;

        std::vector<float> cost(static_cast<std::size_t>(N), std::numeric_limits<float>::infinity());
        std::vector<int> parent(static_cast<std::size_t>(N), -1);
        struct Node {
            float c;
            int idx;
            bool operator<(const Node& o) const { return c > o.c; } // min-heap
        };
        std::priority_queue<Node> pq;

        cost[static_cast<std::size_t>(srcIdx)] = 0.0f;
        pq.push(Node{0.0f, srcIdx});

        auto toXY = [W](int idx, int& x, int& y) {
            x = idx % W;
            y = idx / W;
        };

        while (!pq.empty()) {
            Node n = pq.top();
            pq.pop();
            if (n.idx == dstIdx) break;
            if (n.c > cost[static_cast<std::size_t>(n.idx)]) continue;

            int x, y; toXY(n.idx, x, y);
            for (int k = 0; k < 8; ++k) {
                const int nx = x + kDx8[k];
                const int ny = y + kDy8[k];
                if (nx < 0 || ny < 0 || nx >= W || ny >= Hh) continue;
                const int j = ny * W + nx;
                const bool diag = (kDx8[k] != 0 && kDy8[k] != 0);
                const float dist = diag ? static_cast<float>(std::sqrt(2.0f)) : 1.0f;
                const float hJ = H.elevation[static_cast<std::size_t>(j)];
                const float extra = std::max(0.0f, hJ - spillH);
                const float newCost = n.c + dist + alpha * extra;
                if (newCost < cost[static_cast<std::size_t>(j)]) {
                    cost[static_cast<std::size_t>(j)] = newCost;
                    parent[static_cast<std::size_t>(j)] = n.idx;
                    pq.push(Node{newCost, j});
                }
            }
        }

        if (parent[static_cast<std::size_t>(dstIdx)] == -1) {
            return {};
        }
        std::vector<int> path;
        for (int cur = dstIdx; cur != -1; cur = parent[static_cast<std::size_t>(cur)]) {
            path.push_back(cur);
            if (cur == srcIdx) break;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
}

// Simplified breaching inspired by Schott et al.: connect basins by carving channels instead of globally filling.
void enforceHydrologyConnectivityWithBreaching(Heightfield& H,
                                               float tinyPitFillEps,
                                               float minBasinDepthForBreach,
                                               int   minBasinSizeForBreach) {
    const int W = H.width;
    const int Hh = H.height;
    const int N = W * Hh;
    if (W <= 1 || Hh <= 1 || N == 0) return;

    // 1) Detect pits via a filled copy
    Heightfield H_filled = clone(H);
    priorityFloodFill(H_filled, tinyPitFillEps);

    std::vector<float> delta(static_cast<std::size_t>(N), 0.0f);
    for (int i = 0; i < N; ++i) {
        delta[static_cast<std::size_t>(i)] = H_filled.elevation[static_cast<std::size_t>(i)] - H.elevation[static_cast<std::size_t>(i)];
    }

    // 2) Label basins where delta > 0
    std::vector<int> basinId(static_cast<std::size_t>(N), -1);
    std::vector<BasinStats> basins;
    std::queue<int> q;
    for (int i = 0; i < N; ++i) {
        if (delta[static_cast<std::size_t>(i)] <= 0.0f || basinId[static_cast<std::size_t>(i)] != -1) continue;
        const int b = static_cast<int>(basins.size());
        basins.emplace_back();
        q.push(i);
        basinId[static_cast<std::size_t>(i)] = b;
        while (!q.empty()) {
            const int cur = q.front(); q.pop();
            basins[b].cells.push_back(cur);
            basins[b].maxDelta = std::max(basins[b].maxDelta, delta[static_cast<std::size_t>(cur)]);
            basins[b].minHeight = std::min(basins[b].minHeight, H.elevation[static_cast<std::size_t>(cur)]);

            const int x = cur % W;
            const int y = cur / W;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + kDx8[k];
                const int ny = y + kDy8[k];
                if (nx < 0 || ny < 0 || nx >= W || ny >= Hh) continue;
                const int ni = ny * W + nx;
                if (delta[static_cast<std::size_t>(ni)] <= 0.0f) continue;
                if (basinId[static_cast<std::size_t>(ni)] != -1) continue;
                basinId[static_cast<std::size_t>(ni)] = b;
                q.push(ni);
            }
        }
    }

    if (basins.empty()) {
        return; // no pits
    }

    // 3) Process basins
    const float smallEps = 1e-4f;
    for (std::size_t bi = 0; bi < basins.size(); ++bi) {
        auto& B = basins[bi];
        const int size = static_cast<int>(B.cells.size());
        if (size == 0) continue;

        if (B.maxDelta < minBasinDepthForBreach || size < minBasinSizeForBreach) {
            // Shallow/tiny basin: leave as-is; we accept small local sinks here.
            continue;
        }

        // 4) Find spill candidate (lowest neighbour outside basin) and deep cell (largest delta)
        int deepIdx = -1;
        float bestDelta = -1.0f;
        int spillIdx = -1;
        float spillH = std::numeric_limits<float>::infinity();

        for (int idx : B.cells) {
            const float d = delta[static_cast<std::size_t>(idx)];
            if (d > bestDelta) {
                bestDelta = d;
                deepIdx = idx;
            }
            const int x = idx % W;
            const int y = idx / W;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + kDx8[k];
                const int ny = y + kDy8[k];
                if (nx < 0 || ny < 0 || nx >= W || ny >= Hh) continue;
                const int ni = ny * W + nx;
                if (basinId[static_cast<std::size_t>(ni)] == static_cast<int>(bi)) continue;
                const float hn = H.elevation[static_cast<std::size_t>(ni)];
                if (hn < spillH) {
                    spillH = hn;
                    spillIdx = ni;
                }
            }
        }

        if (deepIdx < 0 || spillIdx < 0) {
            continue; // no valid breach path
        }

        // 5) Carve a breach channel from deep cell to spill cell
        std::vector<int> path = findBreachPath(H, deepIdx, spillIdx);
        if (path.size() < 2) continue;
        const float hDeep = H.elevation[static_cast<std::size_t>(deepIdx)];
        const float hSpill = H.elevation[static_cast<std::size_t>(spillIdx)];
        const std::size_t L = path.size();
        for (std::size_t step = 0; step < L; ++step) {
            const int idx = path[step];
            const float t = (L > 1) ? static_cast<float>(step) / static_cast<float>(L - 1) : 0.0f;
            const float hTarget = (1.0f - t) * hDeep + t * (hSpill - smallEps);
            H.elevation[static_cast<std::size_t>(idx)] = std::min(H.elevation[static_cast<std::size_t>(idx)], hTarget);
        }
    }
}

