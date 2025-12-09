#pragma once

#include "Heightfield.h"
#include "PipelineConfig.h"

Heightfield runMultiScale(
	const Heightfield & base,
	const PipelineConfig & cfg,
	void (*fluvial)(Heightfield &, int, float, float, float),
	void (*thermal)(Heightfield &, int, float, float));
