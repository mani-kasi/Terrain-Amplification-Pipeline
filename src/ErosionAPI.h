#pragma once

#include "Heightfield.h"

// Forward wrappers that call the existing erosion implementations.
// Parameters override the defaults used by those functions.
void runFluvialPass(Heightfield& H, int iters, float Kf, float p, float q);
void runThermalPass(Heightfield& H, int iters, float Kt, float talusDeg);
void runDepositionPass(Heightfield& H, int iters, float Kd, float slopeCutoff);
