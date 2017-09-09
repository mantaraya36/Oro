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
using namespace al;
using namespace std;

#define GRID_SIZE 7
#define INTERACTION_POINTS 32
#define NUM_OFRENDAS 8

// OSC communication between simulator and control
#define CONTROL_IP_ADDRESS "localhost"
#define CONTROL_IN_PORT 10300

#define SIMULATOR_IP_ADDRESS "localhost"
#define SIMULATOR_IN_PORT 10200

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

    void addController(void *data) {};

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



class Granulator {
public:
	Granulator(std::string path, int maxOverlap = 10):
	    mSamples(nullptr), mFrameCounter(0), mMaxOverlap(maxOverlap) {
		gam::SoundFile file(path);
		if (file.openRead()) {
			mNumChannels = file.channels();
			mNumFrames = file.frames();
			std::unique_ptr<float[]> samples(new float[mNumChannels * mNumFrames], std::default_delete<float[]>() );
			file.readAll(samples.get());
			mSamples = (float **) calloc((size_t) mNumChannels, sizeof(float *));
			for (int i = 0; i < mNumChannels; i++) {
				mSamples[i] = (float *) calloc((size_t) mNumFrames, sizeof(float));
				for (int samp = 0; samp < mNumFrames; samp++) {
					mSamples[i][samp] = samples[samp * mNumChannels + i];
				}
			}
		} else {
			std::cout << "Error opening '" << path << "' for reading." << std::endl;
		}
	}

	float operator()(int index = 0) {
		float out = mSamples[index][mFrameCounter++];
		if (mFrameCounter == mNumFrames) {
			mFrameCounter = 0;
		}
		return out;
	}

private:
	float **mSamples;
	int mNumChannels;
	int mNumFrames;

	int mFrameCounter;
	int mMaxOverlap;
};

static const int Nx = 200, Ny = Nx;

inline int indexAt(int x, int y, int z){
    //return (z*Nx + y)*Ny + x;
    return (y*Nx + x)*2 + z; // may give slightly faster accessing
}

typedef struct {
    double lightPhase;
    float dev[GRID_SIZE * GRID_SIZE * GRID_SIZE];

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

    bool down;

} SharedState;

class SharedPainter {
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

    SharedPainter(SharedState *state, ShaderProgram *shader) {
        mState = state;
        mShader = shader;

        mGrid.resize(GRID_SIZE * GRID_SIZE * GRID_SIZE);
		mGridVertical.resize(GRID_SIZE * GRID_SIZE * GRID_SIZE);
		mGridHorizontal.resize(GRID_SIZE * GRID_SIZE * GRID_SIZE);

        // Add a tessellated plane
		addSurface(waterMesh, Nx,Ny);

		for (Mesh &m: mGrid) {
//			addCylinder(m, 0.01, 1);
			addSphere(m, 0.09);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.16, 0.5, al::fold(0.5/(GRID_SIZE * GRID_SIZE * GRID_SIZE), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridVertical) {
			addCylinder(m, 0.02, 1, 24);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.13, 0.5, al::fold(0.5/(GRID_SIZE * GRID_SIZE * GRID_SIZE), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridHorizontal) {
			addCylinder(m, 0.01, 1, 24);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.14, 0.5, al::fold(0.5/(GRID_SIZE * GRID_SIZE * GRID_SIZE), 0.5)+0.5));
			}
		}

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

		state().lightPhase += 1./1800; if(state().lightPhase > 1) state().lightPhase -= 1;
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
        if (state().chaos < 0.3) {
            fogStart = 0.1;
            fogEnd = 0.2;
        }
        g.fog(fogEnd, fogStart, Color(0,0,0, 0.2));

		g.pushMatrix();
		g.scale(2);
		g.translate(- GRID_SIZE/2,- GRID_SIZE/4, -4.0);
		unsigned int count = 0;
		float *dev = state().dev;
		for (int x = 0; x < GRID_SIZE; x++) {
			g.pushMatrix();
			g.translate(x, 0, -*dev/2);
			for (int y = 0; y < GRID_SIZE; y++) {
				g.pushMatrix();
				g.translate(0, y-*dev/2, 0);
				for (int z = 0; z < GRID_SIZE; z++) {
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
        g.translate(0, 1.3, -4.0);
        g.rotate(70, 1, 0, 0);
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

        g.blendOff();
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
