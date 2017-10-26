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


#include "Cuttlebone/Cuttlebone.hpp"

//#define SURROUND
//#define BUILDING_FOR_ALLOSPHERE

using namespace al;
using namespace std;

#define GRID_SIZEX 8
#define GRID_SIZEY 5
#define GRID_SIZEZ 10
#define INTERACTION_POINTS 32

#define NUM_OFRENDAS 8
#define NUM_CASAS 6

#ifdef BUILDING_FOR_ALLOSPHERE

// OSC communication between simulator and control
#define CONTROL_IP_ADDRESS "control.1g"
#define CONTROL_IN_PORT 10300

#define SIMULATOR_IP_ADDRESS "audio.mat.ucsb.edu"
#define SIMULATOR_IN_PORT 10200

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

// Graphics master server
#define GRAPHICS_IP_ADDRESS "localhost"
#define GRAPHICS_IN_PORT 10100

// Graphics slaves port
#define GRAPHICS_SLAVE_PORT 21000

#endif

#define LAGOON_Y 3.0

// Wave function grid size
static const int Nx = 200, Ny = Nx;

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

	Image imagesCasas[NUM_CASAS];
    Texture textureCasas[NUM_CASAS];

	RenderTree mRenderTree;
	RenderTreeHandler mRenderTreeHandler {mRenderTree};

	float fps = 60;

	osc::Recv mRecvFromSimulator;
	osc::Recv mRecvFromGraphicsMaster;

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

		for(int j=0; j<Ny; ++j){
			for(int i=0; i<Nx; ++i){
				int idx = j*Nx + i;
				float r = sqrt((j - (Ny/2))*(j - (Ny/2)) + (i- (Nx/2))*(i- (Nx/2)));
				if (r > Nx) {
					waterMesh.color(Color(0.0, 0.0, 0.0, 0.0));
				} else {
					waterMesh.color(Color(1.0, 1.0, 1.0, 1.0));
				}
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

    void onInit() {

        vector<string> ofrendaImageFiles =
        {"Fotos/MO_01alpha.png",  "Fotos/MO_04alpha.png", "Fotos/MO_07alpha.png",
         "Fotos/MO_02alpha.png",  "Fotos/MO_05alpha.png",  "Fotos/MO_03alpha.png",
         "Fotos/MO_06.png" };
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

        g.blending(true);
        g.blendModeTrans();
		g.depthTesting(true);
        shader().uniform("enableFog", 1);

//		g.nicest();

        shader().uniform("lighting", 1.0);
        shader().uniform("fogCurve", 1.0);
        float fogColor[4] = {0.0f, 0.0f, 0.0f, 0.2f};
        shader().uniform4("fogColor", fogColor, 4);

		float x = cos(7*state().lightPhase*2*M_PI);
		float y = sin(11*state().lightPhase*2*M_PI);
		float z = cos(state().lightPhase*2*M_PI)*0.5 - 0.6;

		mLight.pos(x,y,z);

		// Set up light
		mLight.globalAmbient(RGB(0.1));	// Ambient reflection for all lights
		mLight.ambient(RGB(0));			// Ambient reflection for this light
		mLight.diffuse(RGB(1,1,1));	// Light scattered directly from light
		mLight.attenuation(1,1,0);		// Inverse distance attenuation
//		light.attenuation(1,0,1);		// Inverse-squared distance attenuation

		// Activate light
		mLight();

		// Set up material (i.e., specularity)
		mGoldMaterial.specular(mLight.diffuse()*0.9); // Specular highlight, "shine"
		mGoldMaterial.shininess(50);			// Concentration of specular component [0,128]

		// Activate material
		mGoldMaterial();



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
		fogStart = 0.1;
        fogEnd = 0.2;
        if (state().chaos > 0.2 && state().chaos < 0.5) {
			fogStart += (2 - 0.1) * (state().chaos - 0.1) / 0.3;
			fogEnd += (30 - 0.2) * (state().chaos - 0.1) / 0.3;
		} else if (state().chaos >= 0.4) {
			fogStart = 2;
			fogEnd = 30;
		}

        g.fog(fogEnd, fogStart, Color(0,0,0, 0.2));

		g.pushMatrix();
		g.scale(2);
		g.translate(- GRID_SIZEX/2,- GRID_SIZEY/2, -4.0);
		unsigned int count = 0;
		float *dev = state().dev;
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
					g.rotate(90, 0, 1, 0);
					g.translate(0, *dev, 0);
					g.draw(mGridVertical[count]);
					g.popMatrix();
					g.pushMatrix();
					g.rotate(90,1, 0, 0);
					g.translate(*dev, *dev, 0);
					g.draw(mGridHorizontal[count]);
					g.popMatrix();
					g.popMatrix();
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
//        mtrl.specular(RGB(0));
//        mtrl.shininess(shininess.get());
//        mtrl();
//        light.dir(1,1,1);
//        light();
//        waterMesh.colors()[0] = waterMesh.get();
//        waterMesh.colors()[0] .a = 1.0;
        g.translate(0, LAGOON_Y, -2.0);
        g.rotate(-90, 1, 0, 0);
		float waterScale = 4.5;
		if (state().chaos > 0.6) {
			waterScale += 2.0 * (state().chaos - 0.6) * 0.4;
		}
		g.scale(waterScale, waterScale*waterScale, waterScale);
        g.draw(waterMesh);
        g.popMatrix();

		g.lighting(false);
        shader().uniform("lighting", 0.0);
        shader().uniform("enableFog", 0);
        for (unsigned int i = 0; i < NUM_OFRENDAS; i++) {
            if (state().ofrendas[i]) {
                g.pushMatrix();

                Vec4f posOfrenda = state().posOfrendas[i];
                g.translate(posOfrenda[0], posOfrenda[1], posOfrenda[2] - 4 + i/(float)NUM_OFRENDAS );
                g.rotate(posOfrenda[3], 0, 0, 1);

                g.scale(0.3);
                textureOfrendas[i].bind(0);
                g.draw(mQuad);
                textureOfrendas[i].unbind();

                g.popMatrix();
            }
        }

//		g.lighting(true);
		float casasIndex = state().chaos;
		shader().uniform("lighting", 0.0);
		shader().uniform("texture", casasIndex);
//        shader().uniform("enableFog", 1.0);
		if (state().mouseDown) {
			g.color(casasIndex, casasIndex, casasIndex, 0.5);
		} else {
			g.color(casasIndex* 0.2, 0.0, 0.0, 0.3);
			shader().uniform("texture", casasIndex/3);
		}
		dev = state().dev;

		g.pushMatrix();
		g.rotate(180, 0, 1, 0);

		for (unsigned int i = 0; i < NUM_CASAS; i++) {
			if (true) {
				textureCasas[i].bind(0);
				for (int j = 0; j < 3; j++) {
					g.pushMatrix();
					g.rotate(i*20 + j *120, 0, 0.1, 1);
					g.translate(0.1 + state().chaos, 0, -2 - 0.01 * i);

					g.translate(*dev* 0.01, 0, *dev* 0.1);

//					g.rotate(10, 0.3, 0.12, 0.0);

//					g.translate(0, *dev* 0.15, 0);
					g.rotate(state().casasPhase, 0.64, 0.12, 1);
					//                Vec4f posOfrenda = state().posOfrendas[i];
					//				Vec4f posOfrenda = state().posOfrendas[i];
					//                g.rotate(posOfrenda[3], 0, 0, 1);

					//				g.rotate(i*20, 0, 0, 1);
					g.scale(0.4 + casasIndex* 0.9);
					g.draw(mQuad);
					g.popMatrix();
				}

				for (int j = 0; j < 3; j++) {
					g.pushMatrix();
					g.rotate(i*20 + j *140, 0, 0.1, 1);
					g.translate(0.3 + state().chaos* 2.0, 0, -2 + 0.01 * i);

					g.translate(*dev* 0.01, *dev* 0.05, 0);
					g.rotate((1-casasIndex)*10, 0.3, 0.12, 0.0);

					g.translate(*dev* 0.05, *dev* 0.15, 0);
					g.rotate(state().casasPhase* 1.2124, 0.1, 0.22, 0.6);
					//                Vec4f posOfrenda = state().posOfrendas[i];
					//				Vec4f posOfrenda = state().posOfrendas[i];
					//                g.rotate(posOfrenda[3], 0, 0, 1);

					//				g.rotate(i*20, 0, 0, 1);
					g.scale(casasIndex* 2  + 0.3);
					g.draw(mQuad);
					g.popMatrix();
				}
				textureCasas[i].unbind();
				dev++;
            }
        }

        g.popMatrix();

		shader().uniform("texture", 1.0);
		mRenderTree.render(g);
		g.blendOff();

	}

	virtual void onMessage(osc::Message &m) {
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
        module2->setPosition(Vec3d(randPosX, -0.42, -1));
//                    module2->addBehavior(std::make_shared<FadeIn>(2* fps));
        module2->addBehavior(std::make_shared<Sink2>(4* fps, 0.08));
        module2->addBehavior(std::make_shared<FadeOut>(4* fps,5* fps));

//        auto lineStrip = mRenderTree.createModule<LineStripModule>();
//        for (int i = 0; i < 32; i++) {
//            lineStrip->addVertex(mPairHash.mHashX[i] * window(0).width()/2, mPairHash.mHashY[i] * window(0).height()/2, 0.1);
//        }
//        lineStrip->addBehavior(std::make_shared<FadeOut>(7* fps,3* fps));
    }

    void addBitcoinMarker(std::string text, float x, float y) {
        auto module = mRenderTree.createModule<TextRenderModule>();
        module->setFontSize(24);
        module->setScale(1.0);
        module->setText(text);
		module->setPosition(Vec3d(2 * (x - 0.5), LAGOON_Y - 2, (y * -2) - 3));
        module->addBehavior(std::make_shared<Timeout>(10 * fps));
        module->addBehavior(std::make_shared<Sink2>(10* fps, 3.0 + rnd::gaussian()*0.5));
        module->addBehavior(std::make_shared<FadeOut>(6* fps,15* fps));
    }

    SharedState &state() {return *mState;}
    SharedState *mState;

    ShaderProgram &shader() {return *mShader;}
    ShaderProgram *mShader;
};

static const char* vertexShader = R"(varying vec4 color; varying vec3 normal, lightDir, eyeVec;
         uniform float fogCurve;

          uniform int enableFog;
         /* The fog amount in [0,1] passed to the fragment shader. */
         varying float fogFactor;

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
         //float z = gl_FragCoord.z / gl_FragCoord.w; /* per-frament fog would use this */
         fogFactor = (z - gl_Fog.start) * gl_Fog.scale;
         fogFactor = clamp(fogFactor, 0., 1.);
         if(fogCurve != 0.){
             fogFactor = (1. - exp(-fogCurve*fogFactor))/(1. - exp(-fogCurve));
         }
         }
  })";


static const char* fragmentShader = R"(uniform float lighting; uniform float texture;
                      uniform sampler2D texture0; varying vec4 color;
                      varying vec3 normal, lightDir, eyeVec;
         varying float fogFactor;
          uniform vec4 fogColor;
         uniform int enableFog;
         void main() {

    vec4 colorMixed;
    if (texture > 0.0) {
      vec4 textureColor = texture2D(texture0, gl_TexCoord[0].st);
      colorMixed = mix(color, textureColor, texture);
    } else {
      colorMixed = color;
    }

    vec4 final_color = colorMixed * gl_LightSource[0].ambient;
    vec3 N = normalize(normal);
    vec3 L = lightDir;
    float lambertTerm = max(dot(N, L), 0.0);
    final_color += gl_LightSource[0].diffuse * colorMixed * lambertTerm;
    vec3 E = eyeVec;
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(R, E), 0.0), 0.9 + 1e-20);
    final_color += gl_LightSource[0].specular * spec;
    gl_FragColor = mix(colorMixed, final_color, lighting);

         // fog
         if (enableFog == 1) {
         float c = fogFactor;
         c = (3. - 2.*c) * c*c;		// smooth step
         // c *= c;					// smooth step sqr, lighter fog
         // c = c-1.; c = 1. - c*c;	// parabolic, denser fog

         // vec4 fogCol = texture2D(texture0, fog_xy);
         // vec4 fogCol = vec4(0.0, 0.0, 0.0, 1.0);
         vec4 fogCol = fogColor;

//         // This is required if we want blending to work
//         if(gl_FragColor.a < 0.9999){
//             gl_FragColor.a = gl_FragColor.a * (1.-c);
//             // gl_FragColor = vec4(1,0,0,1); return;
//         }
                               gl_FragColor.rgb = mix(gl_FragColor.rgb, fogCol.rgb, c);
          }
         })";


#endif
