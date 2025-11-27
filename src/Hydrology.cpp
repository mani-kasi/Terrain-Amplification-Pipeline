#include "Hydrology.h"

#include <queue>
#include <vector>
#include <cmath>
#include <cstdint>

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

