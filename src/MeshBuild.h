#pragma once

#include "Heightfield.h"
#include "ofMain.h"

struct TerrainWorld {
	float worldWidth = 1000.0f;
	float worldDepth = 1000.0f;
	float heightScale = 200.0f;
	bool center = true;
};

ofMesh buildTerrainMesh(const Heightfield & H, const TerrainWorld & W);

void colorByHeight(ofMesh & mesh, const Heightfield & H);
void colorByLogDrainage(ofMesh & mesh, const Heightfield & H, const Heightfield & drainageField);
void colorBySlope(ofMesh & mesh, const Heightfield & H, float dx, float dy);
