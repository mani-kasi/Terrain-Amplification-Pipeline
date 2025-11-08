#pragma once

#include "ofMain.h"
#include <vector>
#include <cstdint>

class Heightfield {
public:
    int width = 0;
    int height = 0;

    // elevation in world units (e.g., meters)
    std::vector<float> elevation;

    // hardness mask: 0 = very soft (erodes easily), 1 = very hard (resists erosion)
    std::vector<float> hardness;

    // drainage area / water flux (for later erosion stages)
    std::vector<float> drainage;

    // Allocate all buffers for a given resolution
    void allocate(int w, int h);

    // Index helpers
    int idx(int x, int y) const;

    // Accessors for elevation
    float& h(int x, int y);
    const float& h(int x, int y) const;

    // Accessors for hardness
    float& hard(int x, int y);

    // Accessors for drainage
    float& drain(int x, int y);

    // Reset hardness and drainage masks
    void clearMasks();

    // Load/save heightfield from/into a grayscale PNG
    bool loadFromPng(const std::string& path,
                     float minHeight = 0.0f,
                     float maxHeight = 100.0f);

    bool saveToPng(const std::string& path) const;

    // Generate a synthetic test terrain (sin waves + Gaussian bumps)
    void generateTestTerrain(float minHeight = 0.0f,
                             float maxHeight = 100.0f,
                             std::uint64_t seed = 0);
};

// Upsample elevation to 2x in each dimension using bilinear filtering
Heightfield upsample2xBilinear(const Heightfield& src);

// Add scaled source elevation into destination: dst += s * src
void addScaled(Heightfield& dst, const Heightfield& src, float s);

// Compute element-wise difference of elevation: out = a - b
void sub(const Heightfield& a, const Heightfield& b, Heightfield& out);

// Deep copy of a heightfield
Heightfield clone(const Heightfield& h);
