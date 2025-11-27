#pragma once

#include <vector>

struct ScaleParams {
  int   itersFluvial;   // number of fluvial erosion iterations
  int   itersThermal;   // number of thermal erosion iterations
  float Kf;             // stream power coefficient
  float p;              // drainage exponent
  float q;              // slope exponent
  float Kt;             // thermal coefficient
  float talusDeg;       // angle of repose in degrees
  float blend;          // 0..1: blend amount for this scale
  float Kd;             // deposition coefficient
  float slopeCut;       // slope cutoff threshold for deposition
};

struct PipelineConfig {
  std::vector<ScaleParams> scales; // coarse -> fine
};
