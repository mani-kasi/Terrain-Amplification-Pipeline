#pragma once

#include "Heightfield.h"

// Raise interior pits minimally so water can exit to the boundary (priority-flood).
void priorityFloodFill(Heightfield& H, float epsRaise = 1e-4f);

