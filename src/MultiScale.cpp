#include "MultiScale.h"
#include "Heightfield.h"
#include "ErosionAPI.h"
#include "Hydrology.h"
#include "ofMain.h"

#include <algorithm>
#include <limits>
#include <cmath>

static void minMax(const Heightfield& H, float& mn, float& mx) {
    mn = std::numeric_limits<float>::infinity();
    mx = -std::numeric_limits<float>::infinity();
    const int w = H.width;
    const int h = H.height;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float v = H.h(x, y);
            if (!std::isfinite(v)) continue;
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    }
}

Heightfield runMultiScale(
  const Heightfield& base,
  const PipelineConfig& cfg,
  void (*fluvial)(Heightfield&, int, float, float, float),
  void (*thermal)(Heightfield&, int, float, float))
{
    Heightfield H = clone(base);
    for (std::size_t s = 0; s < cfg.scales.size(); ++s) {
        const auto& S = cfg.scales[s];
        if (s > 0) {
            H = upsample2xBilinear(H);
        }

        Heightfield H_before = clone(H);

        if (fluvial) {
            fluvial(H, S.itersFluvial, S.Kf, S.p, S.q);
        }
        if (thermal) {
            thermal(H, S.itersThermal, S.Kt, S.talusDeg);
        }
        // Light deposition pass after thermal to widen valley floors
        const int depIters = std::max(1, S.itersThermal / 2);
        if (depIters > 0 && S.Kd != 0.0f) {
            const auto tD0 = ofGetElapsedTimeMillis();
            runDepositionPass(H, depIters, S.Kd, S.slopeCut);
            const auto tD1 = ofGetElapsedTimeMillis();
            ofLogNotice() << "[ms]   deposition " << (tD1 - tD0) << " ms";
        }

        // Blend detail: H = H_before + blend * (H - H_before)
        Heightfield delta = clone(H_before);
        sub(H, H_before, delta); // delta = H - H_before
        H = clone(H_before);
        addScaled(H, delta, S.blend);

        // Guardrails
        float mn, mx; minMax(H, mn, mx);
        if (!std::isfinite(mn) || !std::isfinite(mx)) {
            H = clone(H_before);
            break;
        }
    }
    // Final hydrology fix: minimal pit-filling so water can reach the boundary
    priorityFloodFill(H, 1e-4f);
    return H;
}
