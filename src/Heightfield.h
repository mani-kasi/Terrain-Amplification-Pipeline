#pragma once

#include "ofMain.h"
#include <cstdint>
#include <vector>

enum class HardnessPreset {
	Uniform,
	RadialCenterHard,
	RadialEdgeHard,
	Noise
};

class Heightfield {
public:
	int width = 0;
	int height = 0;

	std::vector<float> elevation;

	std::vector<float> hardness;

	std::vector<float> drainage;

	void allocate(int w, int h);

	int idx(int x, int y) const;

	float & h(int x, int y);
	const float & h(int x, int y) const;

	float & hard(int x, int y);

	float & drain(int x, int y);

	void clearMasks();

	void applyHardnessPreset(HardnessPreset preset = HardnessPreset::Noise,
		std::uint64_t seed = 0);

	bool loadFromPng(const std::string & path,
		float minHeight = 0.0f,
		float maxHeight = 100.0f);

	bool saveToPng(const std::string & path) const;

	void generateTestTerrain(float minHeight = 0.0f,
		float maxHeight = 100.0f,
		std::uint64_t seed = 0);
};

Heightfield upsample2xBilinear(const Heightfield & src);

Heightfield upsample2xBicubic(const Heightfield & src);

void addScaled(Heightfield & dst, const Heightfield & src, float s);

void sub(const Heightfield & a, const Heightfield & b, Heightfield & out);

Heightfield clone(const Heightfield & h);
