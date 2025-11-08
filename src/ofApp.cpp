#include "ofApp.h"

#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <sstream>

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

	const float maxDim = static_cast<float>(terrainResolution);
	cellSize = 1.0f;
	cam.setDistance(maxDim * cellSize * 1.5f);
	cam.setNearClip(0.1f);
	cam.setFarClip(10000.0f);

	rebuildTerrainMesh(true);

	fluvialParams.k = 0.0025f;
	fluvialParams.dt = 1.0f;
	fluvialParams.minSlope = 0.001f;
	fluvialParams.intensity = 10.0f;

	thermalParams.talusAngle = 0.6f;
	thermalParams.c = 0.5f;
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
		if (showWireframe) {
			ofSetColor(255);
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
	std::ostringstream hud;
	hud << "Controls:\n";
	hud << "  E: Wireframe [" << (showWireframe ? "ON" : "OFF") << "]\n";
	hud << "  WASD: Move camera\n";
	hud << "  F: Fluvial erosion step\n";
	hud << "  T: Thermal erosion step\n";
	hud << "  R: New terrain (reset)\n";
	hud << "Seed: " << terrainSeed << "\n";
	hud << "Mesh vertices: " << terrainMesh.getNumVertices();

	ofDrawBitmapString(hud.str(), 10, 20);
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

	terrainMesh.clear();
	terrainMesh.setMode(OF_PRIMITIVE_TRIANGLES);

	const int w = terrain.width;
	const int h = terrain.height;
	const float halfW = (w - 1) * 0.5f * cellSize;
	const float halfH = (h - 1) * 0.5f * cellSize;

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const float sx = x * cellSize - halfW;
			const float sz = y * cellSize - halfH;
			const float sy = terrain.h(x, y);

			terrainMesh.addVertex(ofVec3f(sx, sy, sz));
			terrainMesh.addNormal(ofVec3f(0, 1, 0));
		}
	}

	float minE = std::numeric_limits<float>::max();
	float maxE = std::numeric_limits<float>::lowest();
	for (float e : terrain.elevation) {
		minE = std::min(minE, e);
		maxE = std::max(maxE, e);
	}
	const float range = std::max(1e-6f, maxE - minE);

	for (std::size_t i = 0; i < terrainMesh.getNumVertices(); ++i) {
		const float e = terrain.elevation[i];
		const float norm = ofClamp((e - minE) / range, 0.0f, 1.0f);

		ofFloatColor col;
		if (norm < 0.3f) {
			col = ofFloatColor(0.1f, 0.3f + 0.5f * norm, 0.1f);
		} else if (norm < 0.7f) {
			const float u = (norm - 0.3f) / 0.4f;
			col = ofFloatColor(0.4f + 0.3f * u, 0.4f + 0.3f * u, 0.35f + 0.2f * u);
		} else {
			const float u = (norm - 0.7f) / 0.3f;
			col = ofFloatColor(0.8f + 0.2f * u, 0.8f + 0.2f * u, 0.8f + 0.2f * u);
		}

		terrainMesh.addColor(col);
	}

	for (int y = 0; y < h - 1; ++y) {
		for (int x = 0; x < w - 1; ++x) {
			const int i0 = y * w + x;
			const int i1 = y * w + (x + 1);
			const int i2 = (y + 1) * w + x;
			const int i3 = (y + 1) * w + (x + 1);

			terrainMesh.addIndex(i0);
			terrainMesh.addIndex(i2);
			terrainMesh.addIndex(i1);

			terrainMesh.addIndex(i1);
			terrainMesh.addIndex(i2);
			terrainMesh.addIndex(i3);
		}
	}

	recomputeNormals(terrainMesh);
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
		showWireframe = !showWireframe;
	} else if (key == 'r' || key == 'R') {
		showWireframe = false;
		rebuildTerrainMesh(true);
	} else if ((key == 'f' || key == 'F') && terrainReady) {
		computeDrainage(terrain);
		applyFluvialErosion(terrain, fluvialParams);
		rebuildTerrainMesh(false);
	} else if ((key == 't' || key == 'T') && terrainReady) {
		applyThermalErosion(terrain, thermalParams);
		rebuildTerrainMesh(false);
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
