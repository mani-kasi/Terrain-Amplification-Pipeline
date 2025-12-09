#pragma once

#include "Heightfield.h"

void runFluvialPass(Heightfield & H, int iters, float Kf, float p, float q);
void runThermalPass(Heightfield & H, int iters, float Kt, float talusDeg);
void runDepositionPass(Heightfield & H, int iters, float Kd, float slopeCutoff);
