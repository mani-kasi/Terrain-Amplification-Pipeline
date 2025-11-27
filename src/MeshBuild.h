#pragma once

#include "ofMain.h"
#include "Heightfield.h"

struct TerrainWorld {
  float worldWidth  = 1000.0f;  // X size in scene units
  float worldDepth  = 1000.0f;  // Y size in scene units
  float heightScale = 200.0f;   // Up axis scale
  bool  center      = true;     // center at origin
};

// Build a terrain mesh with fixed world extents. Y is up (height); X/Z are ground plane.
ofMesh buildTerrainMesh(const Heightfield& H, const TerrainWorld& W);

// Per-vertex coloring helpers
void colorByHeight(ofMesh& mesh, const Heightfield& H);
void colorByLogDrainage(ofMesh& mesh, const Heightfield& H, const Heightfield& drainageField);
void colorBySlope(ofMesh& mesh, const Heightfield& H, float dx, float dy);
