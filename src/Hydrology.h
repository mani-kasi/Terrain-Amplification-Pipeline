#pragma once

#include "Heightfield.h"

// Raise interior pits minimally so water can exit to the boundary (priority-flood).
void priorityFloodFill(Heightfield& H, float epsRaise = 1e-4f);

// Simplified breaching: connect basins by carving channels instead of globally filling.
void enforceHydrologyConnectivityWithBreaching(Heightfield& H,
                                               float tinyPitFillEps,
                                               float minBasinDepthForBreach,
                                               int   minBasinSizeForBreach);
