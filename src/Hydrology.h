#pragma once

#include "Heightfield.h"

void priorityFloodFill(Heightfield & H, float epsRaise = 1e-4f);

void enforceHydrologyConnectivityWithBreaching(Heightfield & H,
	float tinyPitFillEps,
	float minBasinDepthForBreach,
	int minBasinSizeForBreach);
