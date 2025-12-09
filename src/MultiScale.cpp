#include "MultiScale.h"
#include "Heightfield.h"
#include "ErosionAPI.h"
#include "Hydrology.h"
#include "ofMain.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <string>

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
    const std::uint64_t t0 = ofGetElapsedTimeMillis();
    Heightfield H = clone(base);
    // No global pit-fill here; connectivity will be handled per-scale via breaching.
    for (std::size_t s = 0; s < cfg.scales.size(); ++s) {
        const std::uint64_t tScaleStart = ofGetElapsedTimeMillis();
        const auto& S = cfg.scales[s];
        if (s > 0) {
            H = upsample2xBicubic(H); // sharper than bilinear for multi-scale detail
        }

        Heightfield H_before = clone(H);

        std::uint64_t tUpscaleEnd = ofGetElapsedTimeMillis();
        if (fluvial) {
            const auto tF0 = ofGetElapsedTimeMillis();
            fluvial(H, S.itersFluvial, S.Kf, S.p, S.q);
            const auto tF1 = ofGetElapsedTimeMillis();
            ofLogNotice() << "[ms] fluvial scale " << s << " time=" << (tF1 - tF0) << " ms";
        }
        if (thermal) {
            const auto tT0 = ofGetElapsedTimeMillis();
            thermal(H, S.itersThermal, S.Kt, S.talusDeg);
            const auto tT1 = ofGetElapsedTimeMillis();
            ofLogNotice() << "[ms] thermal scale " << s << " time=" << (tT1 - tT0) << " ms";
        }
        // Light deposition pass after thermal to widen valley floors
        const int depIters = std::max(1, S.itersThermal / 2);
        if (depIters > 0 && S.Kd != 0.0f) {
            const auto tD0 = ofGetElapsedTimeMillis();
            runDepositionPass(H, depIters, S.Kd, S.slopeCut);
            const auto tD1 = ofGetElapsedTimeMillis();
            ofLogNotice() << "[ms] deposition scale " << s << " time=" << (tD1 - tD0) << " ms";
        }

        // Blend detail: H = H_before + blend * (H - H_before)
        Heightfield delta = clone(H_before);
        sub(H, H_before, delta); // delta = H - H_before
        H = clone(H_before);
        addScaled(H, delta, S.blend);

        // Simplified breaching: carve outlets for significant basins instead of globally filling
        const auto tB0 = ofGetElapsedTimeMillis();
        enforceHydrologyConnectivityWithBreaching(
            H,
            1e-5f,   // tiny fill epsilon used only in the detection copy
            0.001f,  // minimum basin depth to breach (more aggressive)
            4        // minimum basin size to breach (more aggressive)
        );
        const auto tB1 = ofGetElapsedTimeMillis();
        const auto tScaleEnd = ofGetElapsedTimeMillis();
        ofLogNotice() << "[ms] scale " << s
                      << " upsample=" << (tUpscaleEnd - tScaleStart) << " ms"
                      << " total=" << (tScaleEnd - tScaleStart) << " ms";
        // Append per-scale timing to perf log
        try {
            ofDirectory::createDirectory("logs", true, true);
            const std::string ts = ofGetTimestampString("%Y-%m-%d %H:%M:%S.%i");
            ofFile file("logs/perf.txt", ofFile::Append);
            if (file.is_open()) {
                file << "[" << ts << "] scale " << s
                     << " upsample=" << (tUpscaleEnd - tScaleStart) << " ms"
                     << " fluvial=" << S.itersFluvial
                     << " thermal=" << S.itersThermal
                     << " deposition=" << depIters
                     << " breach=" << (tB1 - tB0) << " ms"
                     << " total=" << (tScaleEnd - tScaleStart) << " ms"
                     << " grid=" << H.width << "x" << H.height
                     << "\n";
                file.close();
            }
        } catch (...) {
            ofLogWarning() << "[perf-log] per-scale log failed";
        }

        // Guardrails
        float mn, mx; minMax(H, mn, mx);
        if (!std::isfinite(mn) || !std::isfinite(mx)) {
            H = clone(H_before);
            break;
        }
    }
    // No final global pit-fill sculpting: breaching above handles connectivity.
    const std::uint64_t t1 = ofGetElapsedTimeMillis();

    // Append perf log to logs/perf.txt (guarded to avoid exceptions)
    try {
        ofDirectory::createDirectory("logs", true, true);
        const std::string ts = ofGetTimestampString("%Y-%m-%d %H:%M:%S.%i");
        ofFile file("logs/perf.txt", ofFile::Append);
        if (file.is_open()) {
            file << "[" << ts << "] multi-scale total=" << (t1 - t0)
                 << " ms, grid=" << H.width << "x" << H.height << "\n";
            file.close();
        }
    } catch (const std::exception& e) {
        ofLogWarning() << "[perf-log] failed: " << e.what();
    } catch (...) {
        ofLogWarning() << "[perf-log] failed with unknown error";
    }

    return H;
}
