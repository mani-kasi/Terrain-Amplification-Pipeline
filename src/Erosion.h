#pragma once

#include "Heightfield.h"

struct FluvialParams {
	float k = 0.0001f;
	float dt = 1.0f;
	float minSlope = 0.001f;
	float intensity = 1.0f;
};

struct ThermalParams {
	float talusAngle = 0.6f;
	float c = 0.5f;
};

void computeDrainage(Heightfield & hf);

void applyFluvialErosion(Heightfield & hf, const FluvialParams & params);

void applyThermalErosion(Heightfield & hf, const ThermalParams & params);
