#pragma once

#include "Heightfield.h"

// Simplified peak restoration inspired by Schott et al.; only raises where peaks
// were lowered and does not implement general user-driven retargeting.
void runRetargetPeaks(Heightfield& H, const Heightfield& Hbase,
                      int iters = 12, float alpha = 0.20f, float tau = 0.02f);
