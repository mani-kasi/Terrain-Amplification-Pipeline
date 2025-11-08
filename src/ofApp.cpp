#include "ofApp.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <sstream>
#include <vector>
#include <string>

// Simple HUD panel helper: draws translucent background + vertical lines
static void drawHudPanel(const std::vector<std::string>& lines,
                         float x = 10.f, float y = 20.f, float lineH = 16.f,
                         float pad = 6.f) {
    ofPushStyle();
    // Estimate width using monospace bitmap font (~8 px per char)
    constexpr float kCharW = 8.0f;
    float maxW = 0.f;
    for (const auto& s : lines) {
        float w = kCharW * static_cast<float>(s.size());
        if (w > maxW) maxW = w;
    }
    float w = maxW + pad * 2.f;
    float h = lineH * static_cast<float>(lines.size()) + pad * 2.f;

    ofSetColor(0, 0, 0, 150);
    ofDrawRectangle(x - pad, y - pad, w, h);

    ofSetColor(255);
    float yy = y;
    for (const auto& s : lines) {
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

//--------------------------------------------------------------
void ofApp::setup(){
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

    // Fixed world extents independent of resolution (Y-up height)
    terrainWorld = TerrainWorld();
    terrainWorld.worldWidth  = 1000.0f;
    terrainWorld.worldDepth  = 1000.0f;
    terrainWorld.heightScale = 80.0f; // modest while testing
    terrainWorld.center      = true;
    const float maxDim = std::max(terrainWorld.worldWidth, terrainWorld.worldDepth);
    cam.setDistance(maxDim * 1.5f);
	cam.setNearClip(0.1f);
	cam.setFarClip(10000.0f);

    // No custom shaders; using per-vertex colors

    rebuildTerrainMesh(true);
    // keep a coarsest/base copy for multi-scale
    H_base = terrain;

	fluvialParams.k = 0.0025f;
	fluvialParams.dt = 1.0f;
	fluvialParams.minSlope = 0.001f;
	fluvialParams.intensity = 10.0f;

    thermalParams.talusAngle = 0.6f;
    thermalParams.c = 0.5f;

    // Initialize multi-scale pipeline configuration (coarse -> fine)
    cfg.scales = {
        { /*itF*/80, /*itT*/15, /*Kf*/0.015f, /*p*/0.5f, /*q*/1.0f,
          /*Kt*/0.30f, /*talus*/32.0f, /*blend*/0.65f },
        { /*itF*/40, /*itT*/ 8, /*Kf*/0.008f, /*p*/0.5f, /*q*/1.0f,
          /*Kt*/0.20f, /*talus*/32.0f, /*blend*/0.80f }
    };
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){
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
    // Unified HUD panel
    std::vector<std::string> hud;
    hud.push_back("Controls:");
    hud.push_back(std::string("E: Wireframe [") + (wireframeOn ? "ON" : "OFF") + "]");
    hud.push_back("WASD: Move camera");
    hud.push_back("F: Fluvial erosion step");
    hud.push_back("T: Thermal erosion step");
    hud.push_back("U: Upsample 2x (fixed extents)");
    hud.push_back("M: Run multi-scale pipeline");
    hud.push_back("B: View BASE");
    hud.push_back("V: View MULTI-SCALE");
    hud.push_back("");
    hud.push_back("Seed: " + std::to_string(static_cast<unsigned long long>(terrainSeed)));
    hud.push_back("Grid: " + std::to_string(terrain.width) + "x" + std::to_string(terrain.height));
    hud.push_back("World: " + std::to_string(static_cast<int>(terrainWorld.worldWidth)) +
                  " x " + std::to_string(static_cast<int>(terrainWorld.worldDepth)));
    hud.push_back(std::string("View: ") + ((viewMode == ViewMode::Base) ? "Base" : "Multi-Scale"));
    hud.push_back("Mesh vertices: " + std::to_string(static_cast<int>(terrainMesh.getNumVertices())));

    drawHudPanel(hud, 10.f, 20.f, 16.f, 6.f);
}

//--------------------------------------------------------------
void ofApp::rebuildTerrainMesh(bool regenerateTerrain) {
	if (!terrainReady) {
		return;
	}

	if (regenerateTerrain) {
		terrain.allocate(terrainResolution, terrainResolution);
		terrainSeed = randomTerrainSeed();
		terrain.generateTestTerrain(0.0f, 60.0f, terrainSeed);
	}

    if (terrain.width <= 0 || terrain.height <= 0) {
        return;
    }

    // Log grid/world and height range (pre-scale)
    float minE = std::numeric_limits<float>::max();
    float maxE = std::numeric_limits<float>::lowest();
    for (float e : terrain.elevation) {
        if (std::isfinite(e)) {
            minE = std::min(minE, e);
            maxE = std::max(maxE, e);
        }
    }
    ofLogNotice() << "Grid: " << terrain.width << "x" << terrain.height
                  << ", World: " << terrainWorld.worldWidth << "x" << terrainWorld.worldDepth
                  << ", Height range: [" << minE << ", " << maxE << "]";

    terrainMesh = buildTerrainMesh(terrain, terrainWorld);
    terrainMesh.setMode(OF_PRIMITIVE_TRIANGLES);
    ofLogNotice() << "[mesh] verts=" << terrainMesh.getNumVertices()
                  << " idx=" << terrainMesh.getNumIndices()
                  << " mode=" << terrainMesh.getMode();

    // Using per-vertex colors; no shader uniform caching needed
}

//--------------------------------------------------------------
void ofApp::recomputeNormals(ofVboMesh& mesh) {
	auto& normals = mesh.getNormals();
	auto& vertices = mesh.getVertices();
	const auto& indices = mesh.getIndices();

	if (normals.size() != vertices.size()) {
		normals.assign(vertices.size(), glm::vec3(0.0f));
	}

	for (auto& n : normals) {
		n = glm::vec3(0.0f);
	}

	for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
		const ofIndexType i0 = indices[i];
		const ofIndexType i1 = indices[i + 1];
		const ofIndexType i2 = indices[i + 2];

		const glm::vec3& v0 = vertices[i0];
		const glm::vec3& v1 = vertices[i1];
		const glm::vec3& v2 = vertices[i2];

		const glm::vec3 e1 = v1 - v0;
		const glm::vec3 e2 = v2 - v0;
		const glm::vec3 n = glm::cross(e1, e2);

		normals[i0] += n;
		normals[i1] += n;
		normals[i2] += n;
	}

	for (auto& n : normals) {
		const float len = glm::length(n);
		if (len > 0.0f) {
			n /= len;
		}
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (key == 'e' || key == 'E') {
        wireframeOn = !wireframeOn;
        ofLogNotice() << "[view] wireframe = " << (wireframeOn ? "ON" : "OFF");
    } else if (key == 'r' || key == 'R') {
        wireframeOn = false;
        rebuildTerrainMesh(true);
        H_base = terrain; // reset base
        hasMultiScale = false;
        viewMode = ViewMode::Base;
	} else if ((key == 'f' || key == 'F') && terrainReady) {
		computeDrainage(terrain);
		applyFluvialErosion(terrain, fluvialParams);
		rebuildTerrainMesh(false);
    } else if ((key == 't' || key == 'T') && terrainReady) {
        applyThermalErosion(terrain, thermalParams);
        rebuildTerrainMesh(false);
    } else if (key == 'u' || key == 'U') {
        // Upsample the current heightfield and rebuild the mesh
        terrain = upsample2xBilinear(terrain);
        rebuildTerrainMesh(false);
    } else if (key == 'm' || key == 'M') {
        const std::uint64_t t0 = ofGetElapsedTimeMillis();
        H_ms = runMultiScale(H_base, cfg, runFluvialPass, runThermalPass);
        const std::uint64_t t1 = ofGetElapsedTimeMillis();
        ofLogNotice() << "[ms] runtime " << (t1 - t0) << " ms, grid="
                      << H_ms.width << "x" << H_ms.height;
        terrain = H_ms;
        hasMultiScale = true;
        viewMode = ViewMode::MultiScale;
        rebuildTerrainMesh(false);
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

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
