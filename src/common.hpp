#ifndef __COMMON_BLOB__
#define __COMMON_BLOB__


#include <iostream>
#include <memory>

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

//#define SURROUND
using namespace al;
using namespace std;

#define GRID_SIZE 16
#define INTERACTION_POINTS 512
#define NUM_OFRENDAS 32

struct Ofrenda {
    gam::Decay<float> envelope;
};

struct Ofrenda_Data {
    Ofrenda ofrendas[NUM_OFRENDAS];
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


typedef struct {
    double lightPhase;
    float dev[GRID_SIZE * GRID_SIZE * GRID_SIZE];

    float chaos;
    Vec4f interactionPoints[INTERACTION_POINTS];
	al_sec interactionTimes[INTERACTION_POINTS];
    unsigned int interactionEnd = 0;
    unsigned int interactionBegin = 0;

    Vec4f posOfrendas[NUM_OFRENDAS];
    bool ofrendas[NUM_OFRENDAS]; //if ofrendas are on or off
    Nav nav;
} SharedState;

class SharedPainter {
public:

    Light mLight;			// Necessary to light objects in the scene
	Material mGoldMaterial;		// Necessary for specular highlights
    Material mOfrendaMaterial;

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

		for (Mesh &m: mGrid) {
//			addCylinder(m, 0.01, 1);
			addSphere(m, 0.07);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.16, 0.5, al::fold(0.5/(GRID_SIZE * GRID_SIZE * GRID_SIZE), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridVertical) {
			addCylinder(m, 0.01, 1, 24);
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
                SharedPainter::state().ofrendas[counter] = true;
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

		g.nicest();

        shader().uniform("lighting", 1.0);
        shader().uniform("fogCurve", 1.0);
        g.fog(30, 2, Color(0,0,0, 0.2));
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



        mInteractionLine.reset();
		float widths[512];
		float *width_ponter = widths;
        Vec4f *points = state().interactionPoints + state().interactionBegin;
        for (unsigned int i = state().interactionBegin; i != state().interactionEnd; i++) {
            if (i == INTERACTION_POINTS) {
                i = 0;
                points = state().interactionPoints;
            }
			mInteractionLine.vertex(points->x, points->y, 0);
			*width_ponter++ = std::sqrt((*points)[3]) * 0.1f;
			mInteractionLine.color(HSV(0.14f, 0.5f, (*points)[3]/20.0f));
            points++;
		}
		mInteractionLine.primitive(Graphics::TRIANGLE_STRIP);
		mInteractionLine.ribbonize(widths);
		mInteractionLine.smooth();

		g.pushMatrix();

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

		g.pushMatrix();
		g.scale(3.0);
		g.translate(0,0, -0.05);
		g.draw(mInteractionLine);
		g.popMatrix();


//        mOfrendaMaterial();

        g.lighting(false);
        shader().uniform("lighting", 0.0);

        shader().uniform("enableFog", 0);
        for (unsigned int i = 0; i < NUM_OFRENDAS; i++) {
            if (state().ofrendas[i]) {
                g.pushMatrix();

                g.scale(0.3);
                Vec4f posOfrenda = state().posOfrendas[i];
                g.translate(posOfrenda[0], posOfrenda[1], posOfrenda[2] - 12 + 4* i/(float)NUM_OFRENDAS );
                g.rotate(posOfrenda[3], 0, 0, 1);

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



#endif
