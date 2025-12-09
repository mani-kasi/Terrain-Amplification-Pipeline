#include "Retarget.h"
#include "Erosion.h"
#include "Hydrology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

Heightfield upsample2xBilinear(const Heightfield & src);
Heightfield clone(const Heightfield & h);

static inline int Idx(int x, int y, int w) { return y * w + x; }

static Heightfield upsampleToMatch(const Heightfield & src, int W, int H) {
	Heightfield U = clone(src);
	while (U.width < W || U.height < H) {
		U = upsample2xBilinear(U);
	}
	U.width = W;
	U.height = H;
	U.elevation.resize(static_cast<std::size_t>(W) * static_cast<std::size_t>(H));
	return U;
}

static void buildRidgeMask(const Heightfield & U, std::vector<float> & M, float tau) {
	const int W = U.width;
	const int Hh = U.height;
	M.assign(static_cast<std::size_t>(W) * static_cast<std::size_t>(Hh), 0.0f);

	auto get = [&](int x, int y) { return U.elevation[static_cast<std::size_t>(Idx(x, y, W))]; };

	const auto minmax = std::minmax_element(U.elevation.begin(), U.elevation.end());
	const float hmin = (minmax.first != U.elevation.end()) ? *minmax.first : 0.0f;
	const float hmax = (minmax.second != U.elevation.end()) ? *minmax.second : 1.0f;
	const float hrng = std::max(1e-6f, hmax - hmin);

	for (int y = 0; y < Hh; ++y) {
		for (int x = 0; x < W; ++x) {
			const int xm = std::max(0, x - 1), xp = std::min(W - 1, x + 1);
			const int ym = std::max(0, y - 1), yp = std::min(Hh - 1, y + 1);
			const float mean4 = 0.25f * (get(xm, y) + get(xp, y) + get(x, ym) + get(x, yp));
			const float prom = std::max(0.0f, get(x, y) - mean4);
			const float s = prom / hrng;

			const float t0 = tau;
			const float t1 = tau * 2.0f;
			float m;
			if (s <= t0)
				m = 0.0f;
			else if (s >= t1)
				m = 1.0f;
			else
				m = (s - t0) / (t1 - t0);

			M[static_cast<std::size_t>(Idx(x, y, W))] = m;
		}
	}

	std::vector<float> B = M;
	for (int y = 0; y < Hh; ++y) {
		for (int x = 0; x < W; ++x) {
			const int xm = std::max(0, x - 1), xp = std::min(W - 1, x + 1);
			const int ym = std::max(0, y - 1), yp = std::min(Hh - 1, y + 1);
			B[static_cast<std::size_t>(Idx(x, y, W))] = 0.2f * M[static_cast<std::size_t>(Idx(x, y, W))] + 0.2f * M[static_cast<std::size_t>(Idx(xm, y, W))] + 0.2f * M[static_cast<std::size_t>(Idx(xp, y, W))] + 0.2f * M[static_cast<std::size_t>(Idx(x, ym, W))] + 0.2f * M[static_cast<std::size_t>(Idx(x, yp, W))];
		}
	}
	M.swap(B);
}

static void dampMaskInChannels(const Heightfield & Href, std::vector<float> & M) {
	Heightfield A = clone(Href);
	computeDrainage(A);

	const int W = A.width;
	const int Hh = A.height;
	if (W <= 0 || Hh <= 0 || M.size() != static_cast<std::size_t>(W) * static_cast<std::size_t>(Hh)) {
		return;
	}

	std::vector<float> L(static_cast<std::size_t>(W) * static_cast<std::size_t>(Hh));
	float lo = std::numeric_limits<float>::infinity();
	float hi = -std::numeric_limits<float>::infinity();

	for (std::size_t i = 0; i < L.size(); ++i) {
		const float v = std::log10(std::max(1e-6f, A.drainage[i]));
		L[i] = v;
		lo = std::min(lo, v);
		hi = std::max(hi, v);
	}
	const float rng = std::max(1e-6f, hi - lo);

	for (std::size_t i = 0; i < L.size(); ++i) {
		const float t = (L[i] - lo) / rng;
		const float gate = 1.0f - ofClamp((t - 0.55f) / (0.85f - 0.55f), 0.0f, 1.0f);
		M[i] *= gate;
	}
}

void runRetargetPeaks(Heightfield & H, const Heightfield & Hbase,
	int iters, float alpha, float tau) {
	const int W = H.width;
	const int Hh = H.height;
	if (W < 2 || Hh < 2) return;

	Heightfield U = upsampleToMatch(Hbase, W, Hh);

	Heightfield H0 = clone(H);

	const std::size_t N = static_cast<std::size_t>(W) * static_cast<std::size_t>(Hh);
	std::vector<float> R(N), Rnew(N), R0(N);
	for (std::size_t i = 0; i < N; ++i) {
		R[i] = H.elevation[i] - U.elevation[i];
		R0[i] = H0.elevation[i] - U.elevation[i];
	}

	std::vector<float> M;
	buildRidgeMask(U, M, tau);
	dampMaskInChannels(H0, M);

	for (int it = 0; it < iters; ++it) {
		for (int y = 0; y < Hh; ++y) {
			for (int x = 0; x < W; ++x) {
				const int i = Idx(x, y, W);
				const int xm = std::max(0, x - 1), xp = std::min(W - 1, x + 1);
				const int ym = std::max(0, y - 1), yp = std::min(Hh - 1, y + 1);

				const float r = R[static_cast<std::size_t>(i)];
				const float lap = 0.25f * (R[static_cast<std::size_t>(Idx(xm, y, W))] + R[static_cast<std::size_t>(Idx(xp, y, W))] + R[static_cast<std::size_t>(Idx(x, ym, W))] + R[static_cast<std::size_t>(Idx(x, yp, W))]) - r;

				const float pushUp = alpha * M[static_cast<std::size_t>(i)] * (-std::min(r, 0.0f));
				Rnew[static_cast<std::size_t>(i)] = r + 0.10f * lap + pushUp;
			}
		}
		R.swap(Rnew);
	}

	for (std::size_t i = 0; i < N; ++i) {
		R[i] = std::max(R[i], R0[i]);
	}

	for (std::size_t i = 0; i < N; ++i) {
		const float hNew = U.elevation[i] + R[i];
		H.elevation[i] = std::max(hNew, H0.elevation[i]);
	}
}
