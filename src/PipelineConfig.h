#pragma once

#include <vector>

struct ScaleParams {
	int itersFluvial;
	int itersThermal;
	float Kf;
	float p;
	float q;
	float Kt;
	float talusDeg;
	float blend;
	float Kd;
	float slopeCut;
};

struct PipelineConfig {
	std::vector<ScaleParams> scales;
};
