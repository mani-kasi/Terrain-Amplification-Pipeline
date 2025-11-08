#pragma once

#include "ofMain.h"
#include "Heightfield.h"
#include "Erosion.h"
#include "MeshBuild.h"
#include <cstdint>

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

    void rebuildTerrainMesh(bool regenerateTerrain = false);
    void recomputeNormals(ofVboMesh& mesh);

    int terrainResolution = 1024;
    Heightfield terrain;
    FluvialParams fluvialParams;
    ThermalParams thermalParams;
    std::uint64_t terrainSeed = 0;
    ofMesh terrainMesh;
    ofEasyCam cam;
    ofLight dirLight;

    bool wireframeOn = false;
    float cellSize = 1.0f;
    TerrainWorld terrainWorld;
    bool terrainReady = false;
    float cameraMoveStep = 50.0f;
		
};
