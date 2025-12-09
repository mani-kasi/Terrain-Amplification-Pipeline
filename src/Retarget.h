#pragma once

#include "Heightfield.h"

void runRetargetPeaks(Heightfield & H, const Heightfield & Hbase,
	int iters = 12, float alpha = 0.20f, float tau = 0.02f);
