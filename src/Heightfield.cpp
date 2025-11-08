#include "Heightfield.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <array>
#include <random>

namespace {
    constexpr float kDefaultMinRange = 1e-6f;
}

void Heightfield::allocate(int w, int h) {
    width = w;
    height = h;
    const int size = std::max(0, width * height);
    elevation.assign(size, 0.0f);
    hardness.assign(size, 1.0f);
    drainage.assign(size, 0.0f);
}

int Heightfield::idx(int x, int y) const {
#ifndef NDEBUG
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);
#endif
    return y * width + x;
}

float& Heightfield::h(int x, int y) {
    return elevation[idx(x, y)];
}

const float& Heightfield::h(int x, int y) const {
    return elevation[idx(x, y)];
}

float& Heightfield::hard(int x, int y) {
    return hardness[idx(x, y)];
}

float& Heightfield::drain(int x, int y) {
    return drainage[idx(x, y)];
}

void Heightfield::clearMasks() {
    std::fill(hardness.begin(), hardness.end(), 1.0f);
    std::fill(drainage.begin(), drainage.end(), 0.0f);
}

bool Heightfield::loadFromPng(const std::string& path,
                              float minHeight,
                              float maxHeight) {
    ofImage img;
    if (!img.load(path)) {
        return false;
    }

    const int imgW = static_cast<int>(img.getWidth());
    const int imgH = static_cast<int>(img.getHeight());
    if (imgW <= 0 || imgH <= 0) {
        return false;
    }

    allocate(imgW, imgH);

    for (int y = 0; y < imgH; ++y) {
        for (int x = 0; x < imgW; ++x) {
            const ofColor c = img.getColor(x, y);
            const float b = c.getBrightness(); // 0-255
            const float t = b / 255.0f;
            const float e = minHeight + t * (maxHeight - minHeight);
            h(x, y) = e;
        }
    }

    clearMasks();
    return true;
}

bool Heightfield::saveToPng(const std::string& path) const {
    if (width <= 0 || height <= 0 || elevation.empty()) {
        return false;
    }

    ofImage img;
    img.allocate(width, height, OF_IMAGE_GRAYSCALE);

    const auto [minIt, maxIt] = std::minmax_element(elevation.begin(), elevation.end());
    const float minE = (minIt != elevation.end()) ? *minIt : 0.0f;
    const float maxE = (maxIt != elevation.end()) ? *maxIt : 1.0f;
    const float range = std::max(kDefaultMinRange, maxE - minE);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float e = h(x, y);
            float t = (e - minE) / range;
            t = ofClamp(t, 0.0f, 1.0f);
            const unsigned char v = static_cast<unsigned char>(t * 255.0f + 0.5f);
            img.setColor(x, y, ofColor(v));
        }
    }

    img.update();
    return img.save(path);
}

namespace {
    float randomInRange(std::mt19937& rng, float minVal, float maxVal) {
        std::uniform_real_distribution<float> dist(minVal, maxVal);
        return dist(rng);
    }
}

void Heightfield::generateTestTerrain(float minHeight,
                                      float maxHeight,
                                      std::uint64_t seed) {
    if (width <= 0 || height <= 0) {
        allocate(256, 256);
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    std::mt19937 rng;
    if (seed == 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(static_cast<std::mt19937::result_type>(seed));
    }

    std::array<ofVec2f, 5> octaveOffsets;
    for (auto& offset : octaveOffsets) {
        offset.set(randomInRange(rng, -5000.0f, 5000.0f),
                   randomInRange(rng, -5000.0f, 5000.0f));
    }

    const float mountainScale = randomInRange(rng, 2.2f, 3.4f);
    const float broadScale = mountainScale * randomInRange(rng, 0.35f, 0.55f);
    const float broadBlend = randomInRange(rng, 0.25f, 0.45f);
    const float canyonDepth = randomInRange(rng, 0.35f, 0.6f);
    const float canyonWidth = randomInRange(rng, 0.11f, 0.18f);
    const float valleyAmplitude = randomInRange(rng, 0.1f, 0.2f);
    const float valleyFrequency = randomInRange(rng, 0.4f, 0.8f);
    const float valleyPhase = randomInRange(rng, 0.0f, TWO_PI);
    const float detailAmplitude = randomInRange(rng, 0.08f, 0.14f);

    auto ridgedFBM = [&](float x, float y,
                         const std::array<ofVec2f, 5>& offsets) {
        float sum = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float weight = 1.0f;
        for (std::size_t octave = 0; octave < offsets.size(); ++octave) {
            float nx = x * frequency + offsets[octave].x;
            float ny = y * frequency + offsets[octave].y;
            float n = ofNoise(nx, ny);
            n = 1.0f - std::fabs(2.0f * n - 1.0f); // ridged
            n *= n;
            sum += n * amplitude * weight;
            weight = ofClamp(n * 2.0f, 0.0f, 1.0f);
            amplitude *= 0.45f;
            frequency *= 2.0f;
        }
        return sum;
    };

    for (int y = 0; y < height; ++y) {
        const float v = (height > 1) ? static_cast<float>(y) / (height - 1) : 0.0f;
        for (int x = 0; x < width; ++x) {
            const float u = (width > 1) ? static_cast<float>(x) / (width - 1) : 0.0f;

            float ridged = ridgedFBM(u * mountainScale, v * mountainScale, octaveOffsets);
            ridged = std::pow(ofClamp(ridged, 0.0f, 1.0f), 0.95f);
            float broad = ridgedFBM(u * broadScale + 100.0f,
                                    v * broadScale + 200.0f,
                                    octaveOffsets);
            broad = ofClamp(broad, 0.0f, 1.0f);
            ridged = ofLerp(ridged, broad, broadBlend);

            // Macro undulations to avoid repetition
            float macro = ofNoise(u * 0.8f + 15.3f, v * 0.8f + 27.9f);
            float detail = ofSignedNoise(u * 12.0f + 200.0f, v * 12.0f + 400.0f) * detailAmplitude;

            // Carve a winding valley through the landscape
            float valleyAxis = 0.5f + valleyAmplitude * std::sin(v * TWO_PI * valleyFrequency + valleyPhase);
            float distToValley = std::fabs(u - valleyAxis);
            float valleyMask = std::exp(-std::pow(distToValley / canyonWidth, 2.0f));
            float canyon = valleyMask * (canyonDepth + 0.2f * ofNoise(v * 3.0f + 90.0f));

            float hNorm = ridged * (0.6f + 0.4f * macro) + detail;
            hNorm -= canyon; // dig the valley

            // Keep valley floor slightly above absolute zero
            float valleyFloor = 0.15f * (1.0f - valleyMask);
            hNorm = std::max(hNorm, valleyFloor);

            hNorm = ofClamp(hNorm, 0.0f, 1.0f);
            const float e = minHeight + hNorm * (maxHeight - minHeight);
            h(x, y) = e;
        }
    }

    clearMasks();
}

// --------------------------------------------------------------
// Helper functions operating on Heightfield elevation

Heightfield clone(const Heightfield& h) {
    Heightfield c;
    c.width = h.width;
    c.height = h.height;
    c.elevation = h.elevation;
    c.hardness = h.hardness;
    c.drainage = h.drainage;
    return c;
}

void addScaled(Heightfield& dst, const Heightfield& src, float s) {
    if (dst.width != src.width || dst.height != src.height) {
#ifndef NDEBUG
        assert(false && "addScaled: dimension mismatch");
#endif
        const int n = std::min<int>(static_cast<int>(dst.elevation.size()), static_cast<int>(src.elevation.size()));
        for (int i = 0; i < n; ++i) {
            dst.elevation[i] += s * src.elevation[i];
        }
        return;
    }
    const int n = dst.width * dst.height;
    for (int i = 0; i < n; ++i) {
        dst.elevation[i] += s * src.elevation[i];
    }
}

void sub(const Heightfield& a, const Heightfield& b, Heightfield& out) {
    const int w = a.width;
    const int h = a.height;
    if (out.width != w || out.height != h) {
        out.allocate(w, h);
    }
    const int n = std::min<int>(w * h, static_cast<int>(std::min(a.elevation.size(), b.elevation.size())));
    for (int i = 0; i < n; ++i) {
        out.elevation[i] = a.elevation[i] - b.elevation[i];
    }
}

Heightfield upsample2xBilinear(const Heightfield& src) {
    Heightfield dst;
    if (src.width <= 0 || src.height <= 0 || src.elevation.empty()) {
        dst.allocate(0, 0);
        return dst;
    }

    const int W2 = src.width * 2;
    const int H2 = src.height * 2;
    dst.allocate(W2, H2);

    for (int Y = 0; Y < H2; ++Y) {
        const float sy = (static_cast<float>(Y) + 0.5f) * 0.5f - 0.5f;
        const int y0f = static_cast<int>(std::floor(sy));
        int y0 = y0f;
        int y1 = y0f + 1;
        float ty = sy - static_cast<float>(y0f);
        if (y0 < 0) y0 = 0;
        if (y1 < 0) y1 = 0;
        if (y0 >= src.height) y0 = src.height - 1;
        if (y1 >= src.height) y1 = src.height - 1;

        for (int X = 0; X < W2; ++X) {
            const float sx = (static_cast<float>(X) + 0.5f) * 0.5f - 0.5f;
            const int x0f = static_cast<int>(std::floor(sx));
            int x0 = x0f;
            int x1 = x0f + 1;
            float tx = sx - static_cast<float>(x0f);
            if (x0 < 0) x0 = 0;
            if (x1 < 0) x1 = 0;
            if (x0 >= src.width) x0 = src.width - 1;
            if (x1 >= src.width) x1 = src.width - 1;

            const float v00 = src.h(x0, y0);
            const float v10 = src.h(x1, y0);
            const float v01 = src.h(x0, y1);
            const float v11 = src.h(x1, y1);

            const float oneMinusTx = 1.0f - tx;
            const float oneMinusTy = 1.0f - ty;
            const float v = (oneMinusTx * oneMinusTy) * v00 + (tx * oneMinusTy) * v10
                          + (oneMinusTx * ty) * v01      + (tx * ty) * v11;

            dst.h(X, Y) = v;
        }
    }

    // Leave hardness/drainage as defaults for now.
    return dst;
}
