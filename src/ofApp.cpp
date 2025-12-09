#include "ofApp.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

static void drawHudPanel(const std::vector<std::string> & lines,
	float x = 10.f, float y = 20.f, float lineH = 16.f,
	float pad = 6.f) {
	ofPushStyle();
	constexpr float kCharW = 8.0f;
	float maxW = 0.f;
	for (const auto & s : lines) {
		float w = kCharW * static_cast<float>(s.size());
		if (w > maxW) maxW = w;
	}
	float w = maxW + pad * 2.f;
	float h = lineH * static_cast<float>(lines.size()) + pad * 2.f;

	ofSetColor(0, 0, 0, 150);
	ofDrawRectangle(x - pad, y - pad, w, h);

	ofSetColor(255);
	float yy = y;
	for (const auto & s : lines) {
		ofDrawBitmapStringHighlight(s, x, yy);
		yy += lineH;
	}
	ofPopStyle();
}

namespace {
std::uint64_t randomTerrainSeed() {
	return static_cast<std::uint64_t>(ofRandom(0, std::numeric_limits<std::uint32_t>::max()));
}
}

std::string ofApp::hardnessPresetName() const {
	switch (hardnessPreset) {
	case HardnessPreset::Noise:
		return "Noise";
	case HardnessPreset::RadialCenterHard:
		return "Radial center hard";
	case HardnessPreset::RadialEdgeHard:
		return "Radial edge hard";
	case HardnessPreset::Uniform:
		return "Uniform";
	}
	return "Unknown";
}

void ofApp::applyCurrentHardness(Heightfield & h) {
	if (hardnessPreset == HardnessPreset::Noise && hardnessSeed == 0) {
		hardnessSeed = randomTerrainSeed();
	}
	h.applyHardnessPreset(hardnessPreset, hardnessSeed);
}

void ofApp::setup() {
	ofSetVerticalSync(true);
	ofEnableDepthTest();
	ofEnableLighting();
	ofBackground(10, 10, 20);

	dirLight.setup();
	dirLight.enable();
	dirLight.setDirectional();
	dirLight.setOrientation(ofVec3f(45.0f, 45.0f, 0.0f));
	dirLight.setDiffuseColor(ofFloatColor(1.0f, 1.0f, 1.0f));
	dirLight.setSpecularColor(ofFloatColor(0.8f, 0.8f, 0.8f));

	terrainReady = true;
	hardnessSeed = randomTerrainSeed();
	hardnessPreset = HardnessPreset::Noise;

	terrainWorld = TerrainWorld();
	terrainWorld.worldWidth = 256.0f;
	terrainWorld.worldDepth = 256.0f;
	terrainWorld.heightScale = 60.0f;
	terrainWorld.center = true;
	const float maxDim = std::max(terrainWorld.worldWidth, terrainWorld.worldDepth);
	cam.setDistance(maxDim * 1.2f);
	cam.setNearClip(0.1f);
	cam.setFarClip(10000.0f);

	rebuildTerrainMesh(true, true);
	H_base = terrain;

	fluvialParams.k = 0.0025f;
	fluvialParams.dt = 1.0f;
	fluvialParams.minSlope = 0.001f;
	fluvialParams.intensity = 10.0f;

	thermalParams.talusAngle = 0.6f;
	thermalParams.c = 0.5f;

	cfg.scales = {
		{ 32, 6, 0.0120f, 0.5f, 1.0f,
			0.22f, 32.0f, 0.45f,
			0.04f, 0.045f },
		{ 28, 4, 0.0105f, 0.5f, 1.0f,
			0.14f, 32.0f, 0.20f,
			0.02f, 0.035f },
		{ 26, 1, 0.0090f, 0.5f, 1.0f,
			0.08f, 32.0f, 0.05f,
			0.00f, 0.030f },
		{ 28, 0, 0.0085f, 0.5f, 1.0f,
			0.00f, 32.0f, 0.00f,
			0.00f, 0.030f }
	};
}

void ofApp::update() {
}

void ofApp::draw() {
	ofEnableDepthTest();
	ofEnableLighting();
	dirLight.enable();

	cam.begin();
	if (terrainReady) {
		ofSetColor(255);
		if (wireframeOn) {
			ofSetLineWidth(1.5f);
			terrainMesh.drawWireframe();
		} else {
			terrainMesh.draw();
		}
	}
	cam.end();

	dirLight.disable();
	ofDisableLighting();
	ofDisableDepthTest();

	ofSetColor(255);
	std::vector<std::string> hud;
	hud.push_back("Controls:");
	hud.push_back(std::string("E: Wireframe [") + (wireframeOn ? "ON" : "OFF") + "]");
	hud.push_back("WASD: Move camera");
	hud.push_back("R: Reset terrain (same seeds)");
	hud.push_back("N: New seeds + reset terrain");
	hud.push_back("F: Fluvial erosion step");
	hud.push_back("T: Thermal erosion step");
	hud.push_back("P: Deposition step");
	hud.push_back("U: Upsample 2x (fixed extents)");
	hud.push_back("M: Run multi-scale pipeline");
	hud.push_back("B: View BASE");
	hud.push_back("V: View MULTI-SCALE");
	hud.push_back(std::string("H: Cycle hardness preset [") + hardnessPresetName() + "]");
	hud.push_back("G: Cycle color mode (Height / Log-Drainage / Slope)");
	hud.push_back("");
	hud.push_back("Seed: " + std::to_string(static_cast<unsigned long long>(terrainSeed)));
	hud.push_back("Hardness seed: " + std::to_string(static_cast<unsigned long long>(hardnessSeed)));
	hud.push_back("Grid: " + std::to_string(terrain.width) + "x" + std::to_string(terrain.height));
	hud.push_back("World: " + std::to_string(static_cast<int>(terrainWorld.worldWidth)) + " x " + std::to_string(static_cast<int>(terrainWorld.worldDepth)));
	if (!cfg.scales.empty()) {
		const auto & s0 = cfg.scales.front();
		std::ostringstream oss;
		oss << "Fluvial: dh -= Kf*A^p*S^q * hardness (Kf=" << s0.Kf
			<< ", p=" << s0.p
			<< ", q=" << s0.q << ")";
		hud.push_back(oss.str());
	}
	hud.push_back(std::string("View: ") + ((viewMode == ViewMode::Base) ? "Base" : "Multi-Scale"));
	hud.push_back("Mesh vertices: " + std::to_string(static_cast<int>(terrainMesh.getNumVertices())));

	drawHudPanel(hud, 10.f, 20.f, 16.f, 6.f);
}

void ofApp::rebuildTerrainMesh(bool regenerateTerrain, bool reseed) {
	if (!terrainReady) {
		return;
	}

	if (regenerateTerrain) {
		terrain.allocate(terrainResolution, terrainResolution);
		if (reseed || terrainSeed == 0) {
			terrainSeed = randomTerrainSeed();
		}
		terrain.generateTestTerrain(0.0f, 60.0f, terrainSeed);
		if (reseed || hardnessSeed == 0) {
			hardnessSeed = randomTerrainSeed();
		}
		applyCurrentHardness(terrain);
	}

	if (terrain.width <= 0 || terrain.height <= 0) {
		return;
	}

	float minE = std::numeric_limits<float>::max();
	float maxE = std::numeric_limits<float>::lowest();
	float minHard = std::numeric_limits<float>::max();
	float maxHard = std::numeric_limits<float>::lowest();
	for (float e : terrain.elevation) {
		if (std::isfinite(e)) {
			minE = std::min(minE, e);
			maxE = std::max(maxE, e);
		}
	}
	if (!terrain.hardness.empty()) {
		for (float hVal : terrain.hardness) {
			if (std::isfinite(hVal)) {
				minHard = std::min(minHard, hVal);
				maxHard = std::max(maxHard, hVal);
			}
		}
	} else {
		minHard = 1.0f;
		maxHard = 1.0f;
	}
	ofLogNotice() << "Grid: " << terrain.width << "x" << terrain.height
				  << ", World: " << terrainWorld.worldWidth << "x" << terrainWorld.worldDepth
				  << ", Height range: [" << minE << ", " << maxE << "]"
				  << ", Hardness range: [" << minHard << ", " << maxHard << "]";

	terrainMesh = buildTerrainMesh(terrain, terrainWorld);
	terrainMesh.setMode(OF_PRIMITIVE_TRIANGLES);
	ofLogNotice() << "[mesh] verts=" << terrainMesh.getNumVertices()
				  << " idx=" << terrainMesh.getNumIndices()
				  << " mode=" << terrainMesh.getMode();

	switch (colorMode) {
	case ColorMode::Height:
		colorByHeight(terrainMesh, terrain);
		break;
	case ColorMode::LogDrainage: {
		Heightfield A = clone(terrain);
		computeDrainage(A);
		colorByLogDrainage(terrainMesh, terrain, A);
		break;
	}
	case ColorMode::Slope: {
		const float dx = terrainWorld.worldWidth / static_cast<float>(std::max(1, terrain.width - 1));
		const float dy = terrainWorld.worldDepth / static_cast<float>(std::max(1, terrain.height - 1));
		colorBySlope(terrainMesh, terrain, dx, dy);
		break;
	}
	}
	overlayNeedsRecolor = false;
}

void ofApp::recomputeNormals(ofVboMesh & mesh) {
	auto & normals = mesh.getNormals();
	auto & vertices = mesh.getVertices();
	const auto & indices = mesh.getIndices();

	if (normals.size() != vertices.size()) {
		normals.assign(vertices.size(), glm::vec3(0.0f));
	}

	for (auto & n : normals) {
		n = glm::vec3(0.0f);
	}

	for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
		const ofIndexType i0 = indices[i];
		const ofIndexType i1 = indices[i + 1];
		const ofIndexType i2 = indices[i + 2];

		const glm::vec3 & v0 = vertices[i0];
		const glm::vec3 & v1 = vertices[i1];
		const glm::vec3 & v2 = vertices[i2];

		const glm::vec3 e1 = v1 - v0;
		const glm::vec3 e2 = v2 - v0;
		const glm::vec3 n = glm::cross(e1, e2);

		normals[i0] += n;
		normals[i1] += n;
		normals[i2] += n;
	}

	for (auto & n : normals) {
		const float len = glm::length(n);
		if (len > 0.0f) {
			n /= len;
		}
	}
}

void ofApp::keyPressed(int key) {
	if (key == 'e' || key == 'E') {
		wireframeOn = !wireframeOn;
		ofLogNotice() << "[view] wireframe = " << (wireframeOn ? "ON" : "OFF");
	} else if (key == 'g' || key == 'G') {
		colorMode = (colorMode == ColorMode::Height)
			? ColorMode::LogDrainage
			: (colorMode == ColorMode::LogDrainage ? ColorMode::Slope
												   : ColorMode::Height);
		overlayNeedsRecolor = true;
		ofLogNotice() << "[view] colorMode=" << static_cast<int>(colorMode);
		rebuildTerrainMesh(false);
	} else if (key == 'r' || key == 'R') {
		wireframeOn = false;
		rebuildTerrainMesh(true, false);
		H_base = terrain;
		hasMultiScale = false;
		viewMode = ViewMode::Base;
		ofLogNotice() << "[reset] regenerated with same seeds=" << terrainSeed
					  << ", hardnessSeed=" << hardnessSeed;
	} else if (key == 'n' || key == 'N') {
		wireframeOn = false;
		rebuildTerrainMesh(true, true);
		H_base = terrain;
		hasMultiScale = false;
		viewMode = ViewMode::Base;
		ofLogNotice() << "[reset] regenerated with new seeds=" << terrainSeed
					  << ", hardnessSeed=" << hardnessSeed;
	} else if ((key == 'f' || key == 'F') && terrainReady) {
		computeDrainage(terrain);
		applyFluvialErosion(terrain, fluvialParams);
		rebuildTerrainMesh(false);
	} else if ((key == 't' || key == 'T') && terrainReady) {
		applyThermalErosion(terrain, thermalParams);
		rebuildTerrainMesh(false);
	} else if ((key == 'p' || key == 'P') && terrainReady) {
		if (!cfg.scales.empty()) {
			const auto & S = cfg.scales.front();
			const int depIters = std::max(1, S.itersThermal / 2);
			runDepositionPass(terrain, depIters, S.Kd, S.slopeCut);
			ofLogNotice() << "[deposition] iters=" << depIters
						  << " Kd=" << S.Kd
						  << " slopeCut=" << S.slopeCut;
			rebuildTerrainMesh(false);
		} else {
			ofLogWarning() << "[deposition] scales config empty";
		}
	} else if (key == 'u' || key == 'U') {
		terrain = upsample2xBilinear(terrain);
		rebuildTerrainMesh(false);
	} else if (key == 'h' || key == 'H') {
		auto nextPreset = [](HardnessPreset p) {
			switch (p) {
			case HardnessPreset::Noise:
				return HardnessPreset::RadialCenterHard;
			case HardnessPreset::RadialCenterHard:
				return HardnessPreset::RadialEdgeHard;
			case HardnessPreset::RadialEdgeHard:
				return HardnessPreset::Uniform;
			case HardnessPreset::Uniform:
				return HardnessPreset::Noise;
			}
			return HardnessPreset::Noise;
		};
		hardnessPreset = nextPreset(hardnessPreset);
		hardnessSeed = randomTerrainSeed();
		applyCurrentHardness(H_base);
		terrain = H_base;
		hasMultiScale = false;
		viewMode = ViewMode::Base;
		ofLogNotice() << "[hardness] preset=" << hardnessPresetName()
					  << ", seed=" << hardnessSeed;
		rebuildTerrainMesh(false);
	} else if (key == 'm' || key == 'M') {
		const std::uint64_t t0 = ofGetElapsedTimeMillis();
		H_ms = runMultiScale(H_base, cfg, runFluvialPass, runThermalPass);
		const std::uint64_t t1 = ofGetElapsedTimeMillis();
		const std::uint64_t t_ms = t1 - t0;
		ofLogNotice() << "[ms] runtime " << t_ms << " ms, grid="
					  << H_ms.width << "x" << H_ms.height;
		terrain = H_ms;
		hasMultiScale = true;
		viewMode = ViewMode::MultiScale;
		rebuildTerrainMesh(false);
		const std::uint64_t t2 = ofGetElapsedTimeMillis();
		runRetargetPeaks(terrain, H_base, 8, 0.20f, 0.02f);
		const std::uint64_t t3 = ofGetElapsedTimeMillis();
		const std::uint64_t t_retarget = t3 - t2;
		H_ms = terrain;
		rebuildTerrainMesh(false);
		ofLogNotice() << "[retarget] runtime " << t_retarget << " ms";
	} else if (key == 'b' || key == 'B') {
		viewMode = ViewMode::Base;
		terrain = H_base;
		rebuildTerrainMesh(false);
	} else if (key == 'v' || key == 'V') {
		if (hasMultiScale) {
			viewMode = ViewMode::MultiScale;
			terrain = H_ms;
			rebuildTerrainMesh(false);
			ofLogNotice() << "[view] Multi-Scale";
		} else {
			ofLogWarning() << "[view] Multi-Scale not available (run M first)";
		}
	} else if (key == 'w' || key == 'W') {
		cam.dolly(cameraMoveStep);
	} else if (key == 's' || key == 'S') {
		cam.dolly(-cameraMoveStep);
	} else if (key == 'a' || key == 'A') {
		cam.truck(-cameraMoveStep);
	} else if (key == 'd' || key == 'D') {
		cam.truck(cameraMoveStep);
	}
}

void ofApp::keyReleased(int key) {
}

void ofApp::mouseMoved(int x, int y) {
}

void ofApp::mouseDragged(int x, int y, int button) {
}

void ofApp::mousePressed(int x, int y, int button) {
}

void ofApp::mouseReleased(int x, int y, int button) {
}

void ofApp::mouseEntered(int x, int y) {
}

void ofApp::mouseExited(int x, int y) {
}

void ofApp::windowResized(int w, int h) {
}

void ofApp::gotMessage(ofMessage msg) {
}

void ofApp::dragEvent(ofDragInfo dragInfo) {
}
