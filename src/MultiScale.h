#pragma once

#include "Heightfield.h"
#include "PipelineConfig.h"

// Runs a multi-scale erosion pipeline.
// - base: input heightfield
// - cfg:  list of per-scale parameters (coarse -> fine). Each scale upsamples 2x from previous.
// - fluvial: function pointer to a fluvial pass: (H, iters, Kf, p, q)
// - thermal: function pointer to a thermal pass: (H, iters, Kt, talusDeg)
Heightfield runMultiScale(
  const Heightfield& base,
  const PipelineConfig& cfg,
  void (*fluvial)(Heightfield&, int, float, float, float),
  void (*thermal)(Heightfield&, int, float, float));

