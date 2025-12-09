#pragma once

#include "Erosion.h"
#include "ErosionAPI.h"
#include "Heightfield.h"
#include "Hydrology.h"
#include "MeshBuild.h"
#include "MultiScale.h"
#include "PipelineConfig.h"
#include "Retarget.h"
#include "ofMain.h"
#include <cstdint>

class ofApp : public ofBaseApp {

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	void rebuildTerrainMesh(bool regenerateTerrain = false, bool reseed = false);
	void recomputeNormals(ofVboMesh & mesh);
	void applyCurrentHardness(Heightfield & h);
	std::string hardnessPresetName() const;

	int terrainResolution = 256;
	Heightfield terrain;
	FluvialParams fluvialParams;
	ThermalParams thermalParams;
	std::uint64_t terrainSeed = 0;
	std::uint64_t hardnessSeed = 0;
	HardnessPreset hardnessPreset = HardnessPreset::Noise;
	ofMesh terrainMesh;
	ofEasyCam cam;
	ofLight dirLight;

	enum class ColorMode { Height,
		LogDrainage,
		Slope };
	ColorMode colorMode = ColorMode::Height;
	bool overlayNeedsRecolor = true;

	bool wireframeOn = false;
	PipelineConfig cfg;

	Heightfield H_base;
	Heightfield H_ms;
	bool hasMultiScale = false;
	enum class ViewMode { Base,
		MultiScale };
	ViewMode viewMode = ViewMode::Base;
	float cellSize = 1.0f;
	TerrainWorld terrainWorld;
	bool terrainReady = false;
	float cameraMoveStep = 50.0f;
};
