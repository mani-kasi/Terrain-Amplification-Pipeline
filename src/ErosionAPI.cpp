#include "ErosionAPI.h"
#include "Erosion.h"

#include <algorithm>
#include <cmath>

namespace {
    // degrees → radians without relying on M_PI
    constexpr float kDeg2Rad = 0.01745329251994329577f; // pi/180
}

void runFluvialPass(Heightfield& H, int iters, float Kf, float /*p*/, float /*q*/) {
    FluvialParams params;
    params.k = Kf;
    // Leave dt, minSlope, intensity at their current defaults.

    const int steps = std::max(0, iters);
    for (int i = 0; i < steps; ++i) {
        computeDrainage(H);
        applyFluvialErosion(H, params);
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
