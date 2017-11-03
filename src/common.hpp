#ifndef __COMMON_BLOB__
#define __COMMON_BLOB__


#include <iostream>
#include <memory>
#include <functional>

#include "allocore/graphics/al_Shapes.hpp"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "alloGLV/al_ParameterGUI.hpp"
#include "alloutil/al_OmniApp.hpp"

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Envelope.h"

#include "Gamma/ipl.h"
#include "Gamma/scl.h"


#include "Cuttlebone/Cuttlebone.hpp"

//#define SURROUND
#define BUILDING_FOR_ALLOSPHERE

using namespace al;
using namespace std;

#define GRID_SIZEX 7
#define GRID_SIZEY 5
#define GRID_SIZEZ 9
#define INTERACTION_POINTS 32

#define NUM_OFRENDAS 16
#define NUM_CASAS 6

#define LAGOON_Y 3.0

// Wave function grid size
static const int Nx = 180, Ny = Nx;

#ifdef BUILDING_FOR_ALLOSPHERE

// OSC communication between simulator and control
#define CONTROL_IP_ADDRESS "control.1g"
#define CONTROL_IN_PORT 10300

#define SIMULATOR_IP_ADDRESS "audio.mat.ucsb.edu"
#define SIMULATOR_IN_PORT 10200

#define AUDIO_IP_ADDRESS "audio.mat.ucsb.edu"
#define AUDIO_IN_PORT 10201

#define AUDIO2_IP_ADDRESS "audio.mat.ucsb.edu"
#define AUDIO2_IN_PORT 10202

// Graphics master server
#define GRAPHICS_IP_ADDRESS "audio.mat.ucsb.edu"
#define GRAPHICS_IN_PORT 10100

// Graphics slaves port
#define GRAPHICS_SLAVE_PORT 21000

#else

// OSC communication between simulator and control
#define CONTROL_IP_ADDRESS "localhost"
#define CONTROL_IN_PORT 10300

#define SIMULATOR_IP_ADDRESS "localhost"
#define SIMULATOR_IN_PORT 10200

#define AUDIO_IP_ADDRESS "localhost"
#define AUDIO_IN_PORT 10201

#define AUDIO2_IP_ADDRESS "localhost"
#define AUDIO2_IN_PORT 10202

// Graphics master server
#define GRAPHICS_IP_ADDRESS "localhost"
#define GRAPHICS_IN_PORT 10100

// Graphics slaves port
#define GRAPHICS_SLAVE_PORT 21000

#endif

struct Ofrenda {
    gam::Decay<float> envelope;
};

struct Ofrenda_Data {
    Ofrenda ofrendas[NUM_OFRENDAS];
};

class PairHash {
public:
    bool hashComplete() { return mCurrentHashIndex == 32; }
    void nextHash(std::string pair, float x, float y) {
        if (!hashComplete()) {
            mHash.insert(mCurrentHashIndex*2, pair);
            mHashX[mCurrentHashIndex] = x;
            mHashY[mCurrentHashIndex] = y;
            mCurrentHashIndex++;
        }
    }
    void clearHash() {
        mCurrentHashIndex = 0;
        mHash.clear();
    }

    bool isBitcoin() {return false;}

    std::string mHash;
    float mHashX[32], mHashY[32];
    int mCurrentHashIndex {0};
};

class InteractionActor {
public:
    InteractionActor(std::shared_ptr<void> data) {}

    void addController(void *data) {}

private:
    std::shared_ptr<void> mData;
//    function<Type()>
};

class InteractionState {
public:

    template <class T = double>
    void addChildState(InteractionState &state, function<bool(T value)> threshold = [](T value) { return value > 1.0; })
{ }

private:
    vector<InteractionActor *> mActors;
    vector<InteractionState *> mChildStates;
    double mProgress; // 0 -> 1

};

inline int indexAt(int x, int y, int z){
    //return (z*Nx + y)*Ny + x;
    return (y*Nx + x)*2 + z; // may give slightly faster accessing
}

typedef struct {
    double lightPhase = 0.75;
    float dev[GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ];

    float chaos;
    float decay, velocity;
    Vec4f interactionPoints[INTERACTION_POINTS];
	al_sec interactionTimes[INTERACTION_POINTS];
    unsigned int interactionEnd = 0;
    unsigned int interactionBegin = 0;

    Vec4f posOfrendas[NUM_OFRENDAS];
    bool ofrendas[NUM_OFRENDAS]; //if ofrendas are on or off
    Nav nav;
    float wave[Nx*Ny*2];
	int zcurr {0};
	float casasPhase = 0.0f;
    bool mouseDown;

} SharedState;

// -------------- Painter
#include "render_tree.hpp"

class Sink2 : public Behavior {
    friend class RenderModule;
public:
	Sink2(unsigned long numTicks, float zDelta) : Behavior() {
        mNumTicks = numTicks;
        mDelta = zDelta/numTicks;
    }

    virtual void init() override {
        mTargetTicks = mModule->getTicks() + mNumTicks;
    }

    virtual void tick() override {
        if (mModule->getTicks() < mTargetTicks) {
            Vec3f pos = mModule->getPosition();
            pos.y -= mDelta;
            mModule->setPosition(pos);
		} else {
			mDone = true;
		}
    }

private:
    unsigned long mTargetTicks;
    unsigned long mNumTicks;
    float mDelta;
};

class SharedPainter : osc::PacketHandler {
public:

    Light mLight;			// Necessary to light objects in the scene
	Material mGoldMaterial;		// Necessary for specular highlights
    Material mOfrendaMaterial;

    Mesh waterMesh;

    vector<Mesh> mGrid;
	vector<Mesh> mGridVertical;
	vector<Mesh> mGridHorizontal;

    Mesh mInteractionLine;
    Mesh mQuad;

    Image imagesOfrendas[NUM_OFRENDAS];
    Texture textureOfrendas[NUM_OFRENDAS];
	std::vector<float> ofrendaAspects;

	Image imagesCasas[NUM_CASAS];
    Texture textureCasas[NUM_CASAS];

	RenderTree mRenderTree;
	RenderTreeHandler mRenderTreeHandler {mRenderTree};

	float fps = 20;
	float mPreviousChaos = 0.0;

	osc::Recv mRecvFromSimulator;
	osc::Recv mRecvFromGraphicsMaster;

	std::vector<std::shared_ptr<TextRenderModule>> mBitcoinHexes;

	SharedState &state() {return *mState;}
    SharedState *mState;

    ShaderProgram &shader() {return *mShader;}
    ShaderProgram *mShader;

    SharedPainter(SharedState *state, ShaderProgram *shader,
	              uint16_t port = GRAPHICS_IN_PORT,
	              uint16_t masterPort = GRAPHICS_SLAVE_PORT) :
	    mRecvFromSimulator(port), mRecvFromGraphicsMaster(masterPort)
	{
        mState = state;
        mShader = shader;

        mGrid.resize(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ);
		mGridVertical.resize(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ);
		mGridHorizontal.resize(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ);

        // Add a tessellated plane
        addSurface(waterMesh, Nx,Ny);
        // fill color array
        for (int i = 0; i < waterMesh.vertices().size(); i += 1) {
            waterMesh.color(1, 1, 1, 1);
        }

        // required: (a > b) && (a != b)
        auto smoothstep = [] (float a, float b, float x) -> float {
            if (x < a) x = a;
            if (x > b) x = b;
            x = (x - a) / (b - a); // to [0:1]
            return x * x * (3.0 - 2.0 * x);
        };

        float waterMeshHalfWidth = (Nx - 1) / 2.0f;
        float waterMeshHalfHeight = (Ny - 1) / 2.0f;
        for (int j=0; j<Ny; ++j) {
            float y = (j - waterMeshHalfHeight) / waterMeshHalfHeight; // [0:1]
            for (int i=0; i<Nx; ++i) {
                float x = (i - waterMeshHalfWidth) / waterMeshHalfWidth; // [0:1]
                float r = sqrt(x * x + y * y); // [0:sqrt(2)]
                r = smoothstep(0.4, 0.8, r);
                waterMesh.colors()[j*Nx + i] = Color(1-r, 1-r, 1-r, 1-r);
            }
        }


		for (Mesh &m: mGrid) {
//			addCylinder(m, 0.01, 1);
			addSphere(m, 0.09);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.16, 0.5, al::fold(0.5/(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridVertical) {
			addCylinder(m, 0.02, 1, 24);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.13, 0.5, al::fold(0.5/(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridHorizontal) {
			addCylinder(m, 0.01, 1, 24);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.14, 0.5, al::fold(0.5/(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ), 0.5)+0.5));
			}
		}

		// Initialize OSC server
		mRecvFromSimulator.handler(*this);
        mRecvFromSimulator.timeout(0.005);
        mRecvFromSimulator.start();

		// For simplicty I'm routing both simulator and graphics master messages to
		// the same function...

		// Initialize graphics master OSC server
		mRecvFromGraphicsMaster.handler(*this);
        mRecvFromGraphicsMaster.timeout(0.005);
        mRecvFromGraphicsMaster.start();
    }
	~SharedPainter() {
		mRecvFromSimulator.stop();
		mRecvFromGraphicsMaster.stop();
	}

	void reset() {
		mRenderTree.clear();
	}

    void onInit() {

		ofrendaAspects = {
		    741.0	/683.0,
		    921.0	/716.0,
		    654.0	/598.0,
		    556.0	/596.0,
		    557.0	/608.0,
//			473.0	/512.0,
		    559.0	/661.0,
		    454.0	/496.0,
		    1050.0/395.0,
		    386	/673,
		    530	/481,
		    330.0/571.0,
		    454.0	/689.0,
		    299.0	/510.0,
		    625.0	/659.0,
		    736.0	/454.0,
		    945.0	/485.0
		};

        vector<string> ofrendaImageFiles =
        {"Fotos/MO_01alpha.png",
		 "Fotos/MO_02alpha.png",
		 "Fotos/MO_03alpha.png",
		 "Fotos/MO_04alpha.png",
          "Fotos/MO_05alpha.png",
//         "Fotos/MO_06.png "
		 "Fotos/MO_07alpha.png",
		 "Fotos/MO_08alpha.png",
		 "Fotos/MO_09alpha.png",
		 "Fotos/MO_10alpha.png",
		 "Fotos/MO_11alpha.png",
		 "Fotos/MO_12_poporo_alpha.png",
		 "Fotos/MO_13alpha.png",
		 "Fotos/MO_14alpha.png",
		 "Fotos/MO_15alpha.png",
		 "Fotos/MO_16alpha.png",
		 "Fotos/MO_17alpha.png"
 };


        int counter = 0;
        for (string filename: ofrendaImageFiles) {
            if (imagesOfrendas[counter].load(filename)) {
                cout << "Read image from " << filename << "  " << imagesOfrendas[counter].format() << endl;
//                printf("Read image from %s\n", filename);
                textureOfrendas[counter].allocate(imagesOfrendas[counter].array());
                textureOfrendas[counter].submit();
                SharedPainter::state().ofrendas[counter] = false;
            } else {
//                printf("Failed to read image from %s!  Quitting.\n", filename);
                exit(-1);
            }
            counter++;
        }

        mQuad.reset();
        mQuad.primitive(Graphics::TRIANGLE_STRIP);
        mQuad.vertex(-1, -1, 0);
        mQuad.texCoord(0, 0);
        mQuad.vertex( 1, -1,0);
        mQuad.texCoord(1, 0);
        mQuad.vertex(-1,  1, 0);
        mQuad.texCoord(0, 1);
        mQuad.vertex( 1,  1, 0);
        mQuad.texCoord(1, 1);


		std::vector<std::string> mFotos = {
		    "Fotos/5623_155776159.jpg",
		    "Fotos/casas1.jpg",
		    "Fotos/casas5.jpg",
		    "Fotos/destruction-of-the-indies.jpg",
		    "Fotos/dsc01195.jpg",
		    "Fotos/Narratio_Regionum_indicarum_per_Hispanos_Quosdam_devastatarum_verissima_Theodore_de_Bry.jpg"
		};

		counter = 0;
		for (string filename: mFotos) {
			if (imagesCasas[counter].load(filename)) {
				cout << "Read image from " << filename << "  " << imagesOfrendas[counter].format() << endl;
				//                printf("Read image from %s\n", filename);
				textureCasas[counter].allocate(imagesCasas[counter].array());
				textureCasas[counter].submit();
			} else {
				//                printf("Failed to read image from %s!  Quitting.\n", filename);
				exit(-1);
			}
			counter++;
		}

    }

	void setTreeMaster() {
		int port = GRAPHICS_SLAVE_PORT;
#ifdef BUILDING_FOR_ALLOSPHERE
		std::vector<std::string> addresses = {
		    "gr02",
		    "gr03",
		    "gr04",
		    "gr05",
		    "gr06",
		    "gr07",
		    "gr08",
		    "gr09",
		    "gr10",
		    "gr11",
		    "gr12",
		    "gr13",
		    "gr14"
		};
//		std::vector<std::string> addresses = {
//		    "255.255.255.255"
//		};
#else
		std::vector<std::string> addresses = {
		    "localhost"
		};
#endif
		//		mRecvFromGraphicsMaster.stop();
		for (auto relayTo: addresses) {
			mRenderTree.addRelayAddress(relayTo, port);
		}
	}


    void onDraw(Graphics& g){
        // it is important that you set both

		float chaos = state().chaos;

        g.cullFace(false);
        g.blending(true);
        g.blendModeTrans();
		g.depthTesting(true);
        shader().uniform("enableFog", 1);
        shader().uniform("tint", Color{1, 1, 1});

//		g.nicest();

        shader().uniform("lighting", 1.0);
        shader().uniform("texture", 0.0f);
        shader().uniform("fogCurve", 1.0);
        float fogColor[4] = {0.0f, 0.0f, 0.0f, 0.2f};
        shader().uniform4("fogColor", fogColor, 4);

		float x = cos(7*state().lightPhase*2*M_PI);
		float y = sin(11*state().lightPhase*2*M_PI);
		float z = cos(state().lightPhase*2*M_PI)*0.5 - 0.6;

		mLight.pos(x,y,z);

        // Set up light
        mLight.ambient(RGB(0.0));         // Ambient reflection for this light
        mLight.diffuse(RGB(0.8)); // Light scattered directly from light
        mLight.specular(RGB(0.4)); // Light scattered directly from light
        mLight.attenuation(1,0,0);      // Inverse distance attenuation

        // Activate light
        mLight();

        // Set up material (i.e., specularity)
        // mGoldMaterial.specular(mLight.diffuse()*0.9); // Specular highlight, "shine"
        // mGoldMaterial.shininess(50);            // Concentration of specular component [0,128]

        // Activate material
        // mGoldMaterial();
        Material().useColorMaterial(true)();



//        mInteractionLine.reset();
//		float widths[512];
//		float *width_ponter = widths;
//        Vec4f *points = state().interactionPoints + state().interactionBegin;
//        for (unsigned int i = state().interactionBegin; i != state().interactionEnd; i++) {
//            if (i == INTERACTION_POINTS) {
//                i = 0;
//                points = state().interactionPoints;
//            }
//			mInteractionLine.vertex(points->x, points->y, 0);
//			*width_ponter++ = std::sqrt((*points)[3]) * 0.001f;
//			mInteractionLine.color(HSV(0.14f, 0.5f, (*points)[3]/20.0f));
//            points++;
//		}
//		mInteractionLine.primitive(Graphics::TRIANGLE_STRIP);
//		mInteractionLine.ribbonize(widths);
//		mInteractionLine.smooth();


        float fogStart = 2;
        float fogEnd = 30;
		fogStart = 1.0;
        fogEnd = 10.0;

        if (chaos > 0.1 && chaos < 0.4) {
			fogStart += (2 - fogStart) * (chaos - 0.1) / 0.3;
			fogEnd += (30 - fogEnd) * (chaos - 0.1) / 0.3;
		} else if (chaos >= 0.4) {
			fogStart = 2;
			fogEnd = 30;
		}

		float wobble = 0.1 + sin(state().casasPhase*0.015);
		float wobble2 = 0.1 + sin(state().casasPhase*0.012);

        g.fog(fogEnd, fogStart, Color(0,0,0, 0.2));
        shader().uniform("tint", Color{1, 1- chaos, 0}); // put in color here

		g.pushMatrix();
		g.scale(2);
		g.translate(wobble2 - GRID_SIZEX/2,- GRID_SIZEY/1.6, -4.0 + wobble);
		unsigned int count = 0;
		float *dev = state().dev;
		float prevdev = 0;
		for (int x = 0; x < GRID_SIZEX; x++) {
			g.pushMatrix();
			g.translate(x, 0, -*dev/2);
			for (int y = 0; y < GRID_SIZEY; y++) {
				g.pushMatrix();
				g.translate(0, y-*dev/2, 0);
				for (int z = 0; z < GRID_SIZEZ; z++) {
					g.pushMatrix();
					g.translate(*dev/-2.0f, 0.0f, -z + *dev);
					g.draw(mGrid[count]);
					g.pushMatrix();
					g.rotate(90, 0, 1, *dev* chaos );
					g.translate(0, *dev, 0);
					g.draw(mGridVertical[count]);
					g.popMatrix();
					g.pushMatrix();
					g.rotate(90,1, 0, 0);
					g.translate(*dev, *dev - chaos, 0);
					g.draw(mGridHorizontal[count]);
					g.popMatrix();
					g.popMatrix();
					prevdev = *dev;
                    dev++;
					count++;
				}
				g.popMatrix();
			}
			g.popMatrix();
		}
		g.popMatrix();

//        // Interaction line
//		g.pushMatrix();
//		g.translate(0,0, 4);
//		g.scale(0.5);
//		g.draw(mInteractionLine);
//		g.popMatrix();


//        mOfrendaMaterial();

        g.fog(30, 2, Color(0,0,0, 0.2));
        g.pushMatrix();
        g.translate(0, LAGOON_Y - chaos * 2.0, -3.0);
        g.rotate(90, 1, 0, 0);
        float waterScale = 4.5;
        if (chaos > 0.6) {
			waterScale += 3.0 * (chaos - 0.6) * 0.4;
        }
        g.scale(1.4 * waterScale, 1.4 * waterScale, 0.4 * waterScale);
        shader().uniform("tint", Color{1, 1- chaos, 0}); // put in color here
        g.draw(waterMesh);
        g.popMatrix();
        shader().uniform("tint", Color{1, 1, 1});

		// Ofrendas de oro
		g.lighting(false);
        shader().uniform("lighting", 0.0);
        shader().uniform("texture", 1.0);
        shader().uniform("enableFog", 0);
        for (unsigned int i = 0; i < NUM_OFRENDAS; i++) {
            if (state().ofrendas[i]) {
                g.pushMatrix();

                Vec4f posOfrenda = state().posOfrendas[i];
                g.translate(posOfrenda[0], posOfrenda[1], posOfrenda[2] );
                g.rotate(posOfrenda[3], 0, 0, 1);

				g.scale(ofrendaAspects[i], 1.0, 1.0);
                g.scale(0.3);
                textureOfrendas[i].bind(0);
                g.draw(mQuad);
                textureOfrendas[i].unbind();

                g.popMatrix();
            }
        }

//		g.lighting(true);
		float casasIndex = chaos;
		shader().uniform("lighting", 0.0);
		shader().uniform("texture", casasIndex);
//        shader().uniform("enableFog", 1.0);
		if (state().mouseDown) {
			g.color(casasIndex, casasIndex, casasIndex, 0.8);
		} else {
			g.color(casasIndex* 0.2, 0.0, 0.0, 0.3);
			shader().uniform("texture", casasIndex/3);
		}
		dev = state().dev;

		g.pushMatrix();
		g.rotate(180, 0, 1, 0);

		/// Casas Atras
		for (unsigned int i = 0; i < NUM_CASAS; i++) {
			if (true) {
				textureCasas[i].bind(0);
				for (int j = 0; j < 2; j++) {
					g.pushMatrix();
					g.rotate(i*30 + j *180, 0, 0.1, 1);

					g.translate(0.35 + chaos * 2, 0, -2 - 0.01 * i);
					float dev1 = *dev++;
					float dev2 = *dev++;
					g.translate(dev1* 0.04, 0, dev2* 0.1);

//					g.rotate(10, 0.3, 0.12, 0.0);

//					g.translate(0, *dev* 0.15, 0);
					g.rotate(state().casasPhase, 0.64, 0.12, 0.1);
					//                Vec4f posOfrenda = state().posOfrendas[i];
					//				Vec4f posOfrenda = state().posOfrendas[i];
					//                g.rotate(posOfrenda[3], 0, 0, 1);

					//				g.rotate(i*20, 0, 0, 1);
					g.scale(0.2 + casasIndex* 0.6);
					g.draw(mQuad);
					g.popMatrix();
				}

				for (int j = 0; j < 3; j++) {
					g.pushMatrix();
					float dev1 = *dev++;
					float dev2 = *dev++;
					g.translate(dev1* 0.1, dev2* 0.2, 0);
					g.rotate(i*20 + j *140, 0.05, 0.1, 1);
					g.translate(-0.1 + chaos* 1.2, 0, -2 + 0.01 * i);

					g.rotate(i*20 + j *140,-1);

//					g.rotate((1-casasIndex)*2, 0.1, 0.6, 0.0);

//					g.translate(*dev* 0.05, *dev* 0.15, 0);
					g.rotate(state().casasPhase* 1.2124, 0.1, 0.22, 0.2);
					//                Vec4f posOfrenda = state().posOfrendas[i];
					//				Vec4f posOfrenda = state().posOfrendas[i];
					//                g.rotate(posOfrenda[3], 0, 0, 1);

					//				g.rotate(i*20, 0, 0, 1);
					g.scale(casasIndex* 0.7  + 0.05);
					g.draw(mQuad);
					g.popMatrix();
				}
				textureCasas[i].unbind();
//				dev++;
            }
        }
		g.popMatrix();

		if (mPreviousChaos > 0.3 && chaos <= 0.3) {
            mRenderTree.clear();
        }
		shader().uniform("texture", 1.0);

//		shader().uniform("tint", Color{1, 1- chaos, 0}); // put in color here
		mRenderTree.render(g);

//		shader().uniform("tint", Color{1, 1, 1}); // put in color here
		g.blendOff();
		mPreviousChaos = chaos;

	}

	virtual void onMessage(osc::Message &m) {
//		m.print();
		if (m.addressPattern() == "/addBitcoinMarker" && m.typeTags() == "sff") {
			string chars;
			float x,y;
			m >> chars >> x >> y;
			addBitcoinMarker(chars, x, y);
		} else if (m.addressPattern() == "/showBitcoinReport" && m.typeTags() == "si") {
			string chars;
			int isBitcoin;
			m >> chars >> isBitcoin;
			showBitcoinReport(chars, isBitcoin != 0);
		} else {
			mRenderTreeHandler.consumeMessage(m, "");
		}
	}

	void showBitcoinReport(std::string hash, bool isBitcoin) {

		auto module = mRenderTree.createModule<TextRenderModule>();
        module->setFontSize(18);
        module->setScale(0.4);
        module->setText(hash);
		float randPosX = rnd::gaussian() * 1.2;
        module->setPosition(Vec3d(randPosX, -0.4, -1));

        module->addBehavior(std::make_shared<Sink2>(5* fps, -0.08));
        module->addBehavior(std::make_shared<FadeOut>(6* fps,3* fps));

        auto module2 = mRenderTree.createModule<TextRenderModule>();
        module2->setFontSize(18);
        module2->setScale(0.3);
        if (isBitcoin) {
            module2->setText("Bitcoin!!");
        } else {
            module2->setText("Not a Bitcoin");
        }
        module2->setPosition(Vec3d(randPosX, -0.41, -1));
//                    module2->addBehavior(std::make_shared<FadeIn>(2* fps));
        module2->addBehavior(std::make_shared<Sink2>(4* fps, 0.08));
        module2->addBehavior(std::make_shared<FadeOut>(4* fps,5* fps));

//        auto lineStrip = mRenderTree.createModule<LineStripModule>();
//        for (int i = 0; i < 32; i++) {
//            lineStrip->addVertex(mPairHash.mHashX[i] * window(0).width()/2, mPairHash.mHashY[i] * window(0).height()/2, 0.1);
//        }
//        lineStrip->addBehavior(std::make_shared<FadeOut>(7* fps,3* fps));

		std::shared_ptr<LineStripModule> lineStrip = mRenderTree.createModule<LineStripModule>();
		lineStrip->setColor(Color(0.2, 0.6, 0.6, 0.5));
		lineStrip->setUniform(shader().id(), shader().uniform("texture"), 0.0);
		lineStrip->setUniform(shader().id(), shader().uniform("lighting"), 1.0);

		for (auto bcHex: mBitcoinHexes) {
			Vec3f pos = bcHex->getPosition();
			if (pos.z < 0 ) {
				lineStrip->addVertex(pos.x, pos.y, pos.z-0.3);
				//			bcHex->addBehavior(std::make_shared<Timeout>(10 * fps));
				//			std::cout << pos.x << " _ " << pos.z << std::endl;
				bcHex->clearBehaviors();
				bcHex->addBehavior(std::make_shared<FadeOut>(4* fps,6* fps));
			}
		}
//		lineStrip->addBehavior(std::make_shared<Timeout>(10 * fps));
		lineStrip->addBehavior(std::make_shared<FadeOut>(4* fps,6* fps));
		mBitcoinHexes.clear();
    }

    void addBitcoinMarker(std::string text, float x, float y) {

        auto module = mRenderTree.createModule<TextRenderModule>();
        module->setFontSize(24);
        module->setScale(1.0);
        module->setText(text);
		module->setPosition(Vec3d(2 * (x - 0.5), LAGOON_Y - 1, (y * -3.5) - 1.5));
        module->addBehavior(std::make_shared<Sink2>(10* fps, 3.0 + rnd::gaussian()*0.5));
        module->addBehavior(std::make_shared<FadeOut>(6* fps,15* fps));
		mBitcoinHexes.push_back(module);
    }
};

static const char* vertexShader = R"(
uniform float fogCurve;
uniform int enableFog;

/* The fog amount in [0,1] passed to the fragment shader. */
varying float fogFactor;
varying vec4 color;
varying vec3 normal, lightDir, eyeVec;

void main() {
    color = gl_Color;
    vec4 vertex = gl_ModelViewMatrix * gl_Vertex;
    normal = gl_NormalMatrix * gl_Normal;
    vec3 V = vertex.xyz;
    eyeVec = normalize(-V);
    lightDir = normalize(vec3(gl_LightSource[0].position.xyz - V));
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = omni_render(vertex);

     if (enableFog == 1) {
        float z = gl_Position.z;
        /* per-frament fog would use this */
        //float z = gl_FragCoord.z / gl_FragCoord.w;
        fogFactor = (z - gl_Fog.start) * gl_Fog.scale;
        fogFactor = clamp(fogFactor, 0., 1.);
        if(fogCurve != 0.){
            fogFactor = (1. - exp(-fogCurve*fogFactor))/(1. - exp(-fogCurve));
        }
    }
}
)";

static const char* fragmentShader = R"(
uniform float lighting;
uniform float texture;
uniform sampler2D texture0;
uniform vec4 fogColor;
uniform int enableFog;
uniform vec4 tint;

varying vec4 color;
varying vec3 normal, lightDir, eyeVec;
varying float fogFactor;

void main() {

    vec4 colorMixed;
    if (texture > 0.0) {
      vec4 textureColor = texture2D(texture0, gl_TexCoord[0].st);
      colorMixed = mix(color, textureColor, texture);
    } else {
      colorMixed = color;
    }
    colorMixed *= tint;

    float alpha = colorMixed.a;

    vec4 final_color = colorMixed * gl_LightSource[0].ambient;
    vec3 N = normalize(normal);
    vec3 L = lightDir;
    float lambertTerm = max(dot(N, L), 0.0);
    final_color += gl_LightSource[0].diffuse * colorMixed * lambertTerm;
    vec3 E = eyeVec;
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(R, E), 0.0), 0.9 + 1e-20);
    final_color += gl_LightSource[0].specular * spec;
    final_color.a = alpha;
    gl_FragColor = mix(colorMixed, final_color, lighting);

    // fog
    if (enableFog == 1) {
        float c = fogFactor;
        c = (3. - 2.*c) * c*c;      // smooth step
        // c *= c;                  // smooth step sqr, lighter fog
        // c = c-1.; c = 1. - c*c;  // parabolic, denser fog

        // vec4 fogCol = texture2D(texture0, fog_xy);
        // vec4 fogCol = vec4(0.0, 0.0, 0.0, 1.0);
        vec4 fogCol = fogColor;

        // if(gl_FragColor.a < 0.9999){
        //     // This is required if we want blending to work
        //     gl_FragColor.a = gl_FragColor.a * (1.-c);
        //     // gl_FragColor = vec4(1,0,0,1); return;
        // }
        gl_FragColor.rgb = mix(gl_FragColor.rgb, fogCol.rgb, c);
    }
}
)";

#endif
