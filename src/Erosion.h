#pragma once

#include "Heightfield.h"

struct FluvialParams {
    float k = 0.0001f;     // erosion coefficient
    float dt = 1.0f;       // time step
    float minSlope = 0.001f; // minimum effective slope
    float intensity = 1.0f;  // global multiplier for erosion amount
};

struct ThermalParams {
    // Slope / height-difference threshold at which material becomes unstable.
    // This is a relative threshold in "height units", not degrees.
    float talusAngle = 0.6f;

    // Fraction of the excess height (above talus) moved per iteration.
    float c = 0.5f;
};

// Compute drainage area (upstream contributing area) for each cell.
// Result is stored in hf.drainage (each cell >= 1.0).
void computeDrainage(Heightfield& hf);

// Apply a single fluvial erosion step using stream power.
// Assumes computeDrainage(hf) has been called.
void applyFluvialErosion(Heightfield& hf, const FluvialParams& params);

// Apply a thermal erosion / creep step.
// Material on slopes steeper than talusAngle moves from higher to lower neighbours.
void applyThermalErosion(Heightfield& hf, const ThermalParams& params);
