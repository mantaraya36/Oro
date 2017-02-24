/*
Allocore Example: Simple App

Description:
This example demonstrates how to use the App class to create a 3D space with
audio and graphics rendering callbacks.

The App class (and any subclass thereof) has some default keyboard and mouse
controls assigned automatically. These are:

	Keyboard		Action
	'w'				move forward
	'x'				move back
	'a'				move left
	'd'				move right
	'e'				move up
	'c'				move down

	'q'				roll counter-clockwise
	'z'				roll clockwise
	left arrow		turn left
	right arrow		turn right
	up arrow		turn up
	down arrow		turn down

	Mouse
	drag left		turn left
	drag right		turn right
	drag up			turn up
	drag down		turn down

For the camera, the coordinate conventions are:

	-x is left
	+x is right
	-y is down
	+y is up
	-z is forward
	+z is backward

Author:
Lance Putnam, 6/2011, putnam.lance@gmail.com
*/

#include <iostream>
#include <memory>

#include "allocore/graphics/al_Shapes.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "alloGLV/al_ParameterGUI.hpp"
#include "alloutil/al_OmniApp.hpp"

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"

//#define SURROUND
using namespace al;

// Fog vertex shader
static const char * fogVert = R"(
	/* 'fogCurve' determines the distribution of fog between the near and far planes.
	Positive values give more dense fog while negative values give less dense
	fog. A value of	zero results in a linear distribution. */
	uniform float fogCurve;

	/* The fog amount in [0,1] passed to the fragment shader. */
	varying float fogFactor;

	void main(){
		gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
		gl_FrontColor = gl_Color;

		float z = gl_Position.z;
		//float z = gl_FragCoord.z / gl_FragCoord.w; /* per-frament fog would use this */
		fogFactor = (z - gl_Fog.start) * gl_Fog.scale;
		fogFactor = clamp(fogFactor, 0., 1.);
		if(fogCurve != 0.){
			fogFactor = (1. - exp(-fogCurve*fogFactor))/(1. - exp(-fogCurve));
		}
	}
)";

	// Fog fragment shader
	static const char * fogFrag = R"(
		varying float fogFactor;

		void main(){
			gl_FragColor = mix(gl_Color, gl_Fog.color, fogFactor);
		}
	)";

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

#define GRID_SIZE 16
#define INTERACTION_POINTS 512

typedef struct {
    double lightPhase;
    float dev[GRID_SIZE * GRID_SIZE * GRID_SIZE];

    Vec4f interactionPoints[INTERACTION_POINTS];
	al_sec interactionTimes[INTERACTION_POINTS];
    unsigned int interactionEnd = 0;
    unsigned int interactionBegin = 0;
} SharedState;


class SharedPainter {
public:

    Light light;			// Necessary to light objects in the scene
	Material material;		// Necessary for specular highlights

    std::vector<Mesh> mGrid;
	std::vector<Mesh> mGridVertical;
	std::vector<Mesh> mGridHorizontal;

    Mesh mInteractionLine;

    SharedPainter(SharedState *state) {
        mState = state;


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

    void onDraw(Graphics& g){
		g.depthTesting(true);
		g.enable(Graphics::FOG);
		g.blending(true);
		g.nicest();
//		background(Color(0.9, 0.9, 0.9, 0.1));
//		g.clearColor(0.1, 0.1, 0.1, 1.0);
		g.clearColor(0.9, 0.9, 0.9, 0.1);
//		g.clear(Graphics::DEPTH_BUFFER_BIT | Graphics::COLOR_BUFFER_BIT);
		g.lighting(true);

		state().lightPhase += 1./1800; if(state().lightPhase > 1) state().lightPhase -= 1;
		float x = cos(7*state().lightPhase*2*M_PI);
		float y = sin(11*state().lightPhase*2*M_PI);
		float z = cos(state().lightPhase*2*M_PI)*0.5 - 0.6;

		light.pos(x,y,z);

		// Set up light
		light.globalAmbient(RGB(0.1));	// Ambient reflection for all lights
		light.ambient(RGB(0));			// Ambient reflection for this light
		light.diffuse(RGB(1,1,1));	// Light scattered directly from light
		light.attenuation(1,1,0);		// Inverse distance attenuation
//		light.attenuation(1,0,1);		// Inverse-squared distance attenuation

		// Activate light
		light();

		// Set up material (i.e., specularity)
		material.specular(light.diffuse()*0.9); // Specular highlight, "shine"
		material.shininess(50);			// Concentration of specular component [0,128]

		// Activate material
		material();

		// Render
//		mShader.begin();

		// Graphics has a Mesh for temporary use
//		Mesh& m = g.mesh();

//		m.reset();
//		m.primitive(g.TRIANGLES);
//		int N = addWireBox(m, 1.0);
//		for(int i=0; i<N; ++i){
//			m.color(HSV(0.1, 0.5, al::fold(phase + i*0.5/N, 0.5)+0.5));
//		}
//		g.draw(m);

//		g.fog(0.05, 20, Color(0,0,0, 0.2));


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

		g.translate(- GRID_SIZE/2,- GRID_SIZE/4, 0);
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
//		mShader.end();
	}

    SharedState &state() {return *mState;}
    SharedState *mState;
};

// We inherit from App to create our own custom application
class MyApp : public OmniApp {
public:

    SharedState mState;
    SharedPainter mPainter;

    SharedState& state() {return mState;}

    int mMouseX, mMouseY;

	ShaderProgram mShader;
	std::vector<gam::NoiseBrown<> > mNoise;
	gam::Domain mVideoDomain;
	std::vector<gam::Biquad<> > mFilters;
	Parameter mChaos;
	Parameter mSpeedX, mSpeedY, mSpeedZ;

	Granulator granX, granY, granZ;
	Granulator background1;
	Granulator background2;
	Granulator background3;
	gam::SineR<float> fluctuation1, fluctuation2;

	Parameter gainBackground1 {"background1", "", 0.2f, "", 0.0f, 1.0f};
	Parameter gainBackground2 {"background2", "", 0.2f, "", 0.0f, 1.0f};
	Parameter gainBackground3 {"background3", "", 0.2f, "", 0.0f, 1.0f};

	ParameterGUI mParameterGUI;

	// This constructor is where we initialize the application
	MyApp(): mVideoDomain(30),
	    mChaos("chaos", "", 0),
	    mSpeedX("speedX", "", 0),
	    mSpeedY("speedY", "", 0),
	    mSpeedZ("speedZ", "", 0),
	    granX("Bounced Files/Piezas oro 1.wav"),
	    granY("Bounced Files/Piezas oro 2.wav"),
	    granZ("Bounced Files/Piezas oro 2.wav"),
	    background1("Bounced Files/Bajo agua.wav"),
	    background2("Bounced Files/Bajo agua.wav"),
	    background3("Bounced Files/Bajo agua.wav"),
        mPainter(&mState)
	{
		AudioDevice::printAll();

		// Configure the camera lens
		lens().near(0.1).far(25).fovy(45);

		// Set navigation position and orientation

		initWindow(Window::Dim(0,0, 600,400), "Untitled", 30);

		// Set background color
		//stereo().clearColor(HSV(0,0,1));
#ifdef SURROUND
		audioIO().device(5);
		initAudio(48000, 256, 8, 0);
#else
		audioIO().device(0);
		initAudio("default", 48000, 64, 0, 2);
#endif

        mNoise.resize(GRID_SIZE * GRID_SIZE * GRID_SIZE);
		mFilters.resize(GRID_SIZE * GRID_SIZE * GRID_SIZE);

		for (gam::Biquad<> &b:mFilters) {
			b.domain(mVideoDomain);
			b.freq(0.3);
		}

		fluctuation1.freq(0.2f);
		fluctuation2.freq(0.3f);

//		mParameterGUI.setParentApp(this);
//		mParameterGUI << new glv::Label("Parameter GUI example");
//		mParameterGUI << gainBackground1 << gainBackground2 << gainBackground3;

		std::cout << "Constructor done" << std::endl;
	}

	virtual bool onCreate() override {
		mShader.compile(fogVert, fogFrag);
        OmniApp::onCreate();

        nav().pos(0,0,10);
		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);
        return true;
	}

	virtual void onSound(AudioIOData& io) override {

		// Things here occur at block rate...

		float mouseSpeedScale = 50.0f;
		while(io()){
			//float in = io.in(0);
			float fluct1 = fluctuation1();
			float fluct2 = fluctuation2();

			float out1 = granX() * mSpeedX.get() * mouseSpeedScale;
			float out2 = granY() * mSpeedY.get() * mouseSpeedScale;

			float bg1 = background1() * gainBackground1.get();
			float bg2 = background2() * gainBackground2.get();
			float bg3 = background3() * gainBackground3.get();

#ifdef SURROUND
			io.out(2) = out1 + (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0) + (bg3 * (1 - fluct2)/2.0);
			io.out(3) = out2 + (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct1)/2.0) + (bg3 * (1 - fluct1)/2.0);

			io.out(6) = (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0) + (bg3 * (1 - fluct2)/2.0);
			io.out(7) = (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct2)/2.0) + (bg3 * (1 - fluct2)/2.0);
#else
			io.out(0) = out1 + (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0) + (bg3 * (1 - fluct2)/2.0);
			io.out(1) = out2 + (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct1)/2.0) + (bg3 * (1 - fluct1)/2.0);
#endif
		}
	}

	virtual void onAnimate(double dt) override {

		al_sec curTime = al_steady_time();
		float mouseSpeed = 100.0 * sqrt(mSpeedX.get() * mSpeedX.get() + mSpeedY.get() * mSpeedY.get() + mSpeedZ.get() * mSpeedZ.get());
		if (mouseSpeed > 2) {
			mChaos.set(mouseSpeed/10.0);
			if ((int) state().interactionEnd - (int) state().interactionBegin != -1) {
				const Vec4d &lastPoint = state().interactionPoints[state().interactionEnd];
				if (lastPoint.x != mMouseX && lastPoint.y != mMouseY) {
					Vec4d newPoint(mMouseX, mMouseY, 0, mouseSpeed);
                    state().interactionEnd++;
                    if (state().interactionEnd == INTERACTION_POINTS) {
                        state().interactionEnd = 0;
                    }
					state().interactionPoints[state().interactionEnd] = newPoint;
					state().interactionTimes[state().interactionEnd] = curTime;
				}
			}
		} else {
			mChaos.set(0.0);
		}
        float timeOut = 5.0;
		for (int it = state().interactionBegin; it != state().interactionEnd; it++) {
            if (it == INTERACTION_POINTS) {
                it = 0;
            }
			if (curTime - state().interactionTimes[it] > timeOut) {
				state().interactionBegin++;
                if (state().interactionBegin == INTERACTION_POINTS) {
                    state().interactionBegin = 0;
                }
			} else {
				break;
			}

		}

        float * dev_ = state().dev;
        unsigned int count = 0;
        for (int x = 0; x < GRID_SIZE; x++) {
			for (int y = 0; y < GRID_SIZE; y++) {
				for (int z = 0; z < GRID_SIZE; z++) {
					*dev_++ = mFilters[count](mNoise[count]() * (mChaos.get() * 5.0f));
					count++;
				}
			}
		}
	}

	virtual void onDraw(Graphics& g) override {
        lens().far(15).near(0.01);
        Color bgColor(0.1f, 0.1f, 0.1f, 0.1f);
		g.fog(lens().far(), lens().near()+1, bgColor);

        mPainter.onDraw(g);
	}


	// This is called whenever a key is pressed.
	virtual bool onKeyDown(const Keyboard& k) override {

		// Use a switch to do something when a particular key is pressed
		switch(k.key()){

		// For printable keys, we just use its character symbol:
		case '1': printf("Pressed 1.\n"); break;
		case 'y': printf("Pressed y.\n"); break;
		case 'n': printf("Pressed n.\n"); break;
		case '.': printf("Pressed period.\n"); break;
		case ' ': printf("Pressed space bar.\n");
			mChaos.set(mChaos.get() > 1.0 ? 0.0 : mChaos.get() + 0.1);
			break;

		// For non-printable keys, we have to use the enums described in the
		// Keyboard class:
		case Keyboard::RETURN: printf("Pressed return.\n"); break;
		case Keyboard::DELETE: printf("Pressed delete.\n"); break;
		case Keyboard::F1: printf("Pressed F1.\n"); break;
		}
        return true;
	}

	// This is called whenever a mouse button is pressed.
	virtual bool onMouseDown(const Mouse& m) override {
		switch(m.button()){
		case Mouse::LEFT: printf("Pressed left mouse button.\n"); break;
		case Mouse::RIGHT: printf("Pressed right mouse button.\n"); break;
		case Mouse::MIDDLE: printf("Pressed middle mouse button.\n"); break;
		}
        return true;
	}

	// This is called whenever the mouse is dragged.
	virtual bool onMouseDrag(const Mouse& m) override {
		// Get mouse coordinates, in pixels, relative to top-left corner of window
		int x = m.x();
		int y = m.y();
		printf("Mouse dragged: %3d, %3d\n", x,y);
        return true;
	}
    virtual bool onMouseMove(const Mouse &m) override {
        mMouseX = (m.x() - (float)width()/2)/(float)width();
        mMouseY = -(m.y() - (float)height()/2)/(float)height();

        mSpeedX.set(m.dx()/(float)width());
		mSpeedY.set(m.dy()/(float)height());
        return true;
    }


	// *****************************************************
	// NOTE: check the App class for more callback functions

};


int main(){
	MyApp().start();
}
