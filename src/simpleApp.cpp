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

#include "allocore/io/al_App.hpp"
#include "allocore/graphics/al_Shapes.hpp"
#include "allocore/ui/al_Parameter.hpp"

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

// We inherit from App to create our own custom application
class MyApp : public App {
public:

	double phase;
	double mLightPhase;
	int mGridSize;
	ShaderProgram mShader;
	std::vector<gam::NoiseBrown<> > mNoise;
	gam::Domain mVideoDomain;
	std::vector<gam::Biquad<> > mFilters;
	std::vector<Mesh> mGrid;
	std::vector<Mesh> mGridVertical;
	std::vector<Mesh> mGridHorizontal;
	Mesh mInteractionLine;
	Parameter mChaos;
	Parameter mSpeedX, mSpeedY, mSpeedZ;

	std::list<Vec4d> mInteractionPoints;
	std::list<al_sec> mInteractionTimes;
	Light light;			// Necessary to light objects in the scene
	Material material;		// Necessary for specular highlights

	Granulator granX, granY, granZ;
	Granulator background1;
	Granulator background2;
	Granulator background3;
	gam::SineR<float> fluctuation1, fluctuation2;

	Parameter gainBackground1;
	Parameter gainBackground2;
	Parameter gainBackground3;

	// This constructor is where we initialize the application
	MyApp(): phase(0), mVideoDomain(30),
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
	    gainBackground1("background1", "", 0.2),
	    gainBackground2("background2", "", 0.2),
	    gainBackground3("background3", "", 0.2)
	{
		AudioDevice::printAll();

		// Configure the camera lens
		lens().near(0.1).far(25).fovy(45);

		// Set navigation position and orientation
		nav().pos(0,0,4);
		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);

		initWindow(Window::Dim(0,0, 600,400), "Untitled", 30);

		// Set background color
		//stereo().clearColor(HSV(0,0,1));
#ifdef SURROUND
		audioIO().device(5);
		initAudio(48000, 256, 8, 0);
#else
		audioIO().device(10);
		initAudio(48000, 64, 2, 0);
#endif

		mGridSize = 16;
		mGrid.resize(mGridSize * mGridSize * mGridSize);
		mGridVertical.resize(mGridSize * mGridSize * mGridSize);
		mGridHorizontal.resize(mGridSize * mGridSize * mGridSize);

		mNoise.resize(mGrid.size());
		mFilters.resize(mNoise.size());
		for (Mesh &m: mGrid) {
//			addCylinder(m, 0.01, 1);
			addSphere(m, 0.07);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.16, 0.5, al::fold(phase + 0.5/(mGridSize * mGridSize * mGridSize), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridVertical) {
			addCylinder(m, 0.01, 1, 24);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.13, 0.5, al::fold(phase + 0.5/(mGridSize * mGridSize * mGridSize), 0.5)+0.5));
			}
		}
		for (Mesh &m: mGridHorizontal) {
			addCylinder(m, 0.01, 1, 24);
			m.generateNormals();
			int cylinderVertices = m.vertices().size();
			for(int i = 0; i < cylinderVertices; i++) {
				m.color(HSV(0.14, 0.5, al::fold(phase + 0.5/(mGridSize * mGridSize * mGridSize), 0.5)+0.5));
			}
		}
		for (gam::Biquad<> &b:mFilters) {
			b.domain(mVideoDomain);
			b.freq(0.3);
		}

		fluctuation1.freq(0.2f);
		fluctuation2.freq(0.3f);

		std::cout << "Constructor done" << std::endl;
	}

	void onCreate(const ViewpointWindow& win){
		mShader.compile(fogVert, fogFrag);
	}

	// This is the audio callback
	virtual void onSound(AudioIOData& io){

		// Things here occur at block rate...

		// This is the sample loop
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

	virtual void onAnimate(double dt){
		// The phase will ramp from 0 to 1 over 1 second. We will use it to
		// animate the color of a sphere.

		al_sec curTime = al_steady_time();
		phase += dt/10.0;
		if(phase >= 1.) phase -= 1.;
		float mousex = (window().mouse().x() - (float)window().width()/2)/(float)window().width();
		float mousey = -(window().mouse().y() - (float)window().height()/2)/(float)window().height();
		float speedx = window().mouse().dx()/(float)window().width();
		float speedy = window().mouse().dy()/(float)window().height();
		float speedz = 0;
		float mouseSpeed = 100.0 * sqrt(speedx * speedx + speedy * speedy + speedz* speedz);
		mSpeedX.set(speedx);
		mSpeedY.set(speedy);
		mSpeedZ.set(speedz);
		if (mouseSpeed > 2) {
			mChaos.set(mouseSpeed/10.0);
			if (mInteractionPoints.size() > 0) {
				const Vec4d &lastPoint = mInteractionPoints.back();
				if (lastPoint.x != mousex && lastPoint.y != mousey) {
					Vec4d newPoint(mousex, mousey, 0, mouseSpeed);
					mInteractionPoints.push_back(newPoint);
					mInteractionTimes.push_back(curTime);
				}
				if (mInteractionPoints.size() > 512) {
					mInteractionPoints.pop_front();
					mInteractionTimes.pop_front();
				}
			} else {
				Vec4d newPoint(mousex, mousey, 0, sqrt(speedx * speedx + speedy * speedy));
				mInteractionPoints.push_back(newPoint);
				mInteractionTimes.push_back(curTime);
			}
		} else {
			mChaos.set(0.0);
		}
		int removeCounter = 0;
		for (auto it = mInteractionTimes.begin(); it != mInteractionTimes.end(); it++) {
			if (curTime - *it > 5.0) {
				removeCounter++;
			} else {
				break;
			}
		}
		for (int i = 0; i < removeCounter; i++) {
			mInteractionPoints.pop_front();
			mInteractionTimes.pop_front();
		}

		mInteractionLine.reset();
		float widths[512];
		float *width_ponter = widths;
		for (auto it = mInteractionPoints.begin(); it != mInteractionPoints.end(); it++) {
			mInteractionLine.vertex(it->x, it->y, 0);
			*width_ponter++ = sqrt((*it)[3]) * 0.1;
			mInteractionLine.color(HSV(0.14, 0.5, (*it)[3]/20));
		}
		mInteractionLine.primitive(Graphics::TRIANGLE_STRIP);
		mInteractionLine.ribbonize(widths);
		mInteractionLine.smooth();
	}

	virtual void onDraw(Graphics& g, const Viewpoint& v){
		g.depthTesting(true);
		g.enable(Graphics::FOG);
		g.blending(true);
		g.nicest();
//		background(Color(0.9, 0.9, 0.9, 0.1));
//		g.clearColor(0.1, 0.1, 0.1, 1.0);
		g.clearColor(0.9, 0.9, 0.9, 0.1);
//		g.clear(Graphics::DEPTH_BUFFER_BIT | Graphics::COLOR_BUFFER_BIT);
		lens().far(15).near(0.01);
		g.fog(lens().far(), lens().near()+1, App::background());
		g.lighting(true);

		mLightPhase += 1./1800; if(mLightPhase > 1) mLightPhase -= 1;
		float x = cos(7*mLightPhase*2*M_PI);
		float y = sin(11*mLightPhase*2*M_PI);
		float z = cos(mLightPhase*2*M_PI)*0.5 - 0.6;

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
		g.pushMatrix();

		g.translate(- mGridSize/2,- mGridSize/4, 0);
		int count = 0;
		float dev = 0.0;
		for (int x = 0; x < mGridSize; x++) {
			g.pushMatrix();
			g.translate(x, 0, -dev/2);
			for (int y = 0; y < mGridSize; y++) {
				g.pushMatrix();
				g.translate(0, y-dev/2, 0);
				for (int z = 0; z < mGridSize; z++) {
					dev = mFilters[count](mNoise[count]() * (mChaos.get() * 5.0));
					g.pushMatrix();
					g.translate(-dev/2, 0, -z + dev);
					g.draw(mGrid[count]);
					g.pushMatrix();
					g.rotate(90, 0, 1, 0);
					g.translate(0, dev, 0);
					g.draw(mGridVertical[count]);
					g.popMatrix();
					g.pushMatrix();
					g.rotate(90,1, 0, 0);
					g.translate(dev, dev, 0);
					g.draw(mGridHorizontal[count]);
					g.popMatrix();
					g.popMatrix();
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


	// This is called whenever a key is pressed.
	virtual void onKeyDown(const ViewpointWindow& w, const Keyboard& k){

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
	}

	// This is called whenever a mouse button is pressed.
	virtual void onMouseDown(const ViewpointWindow& w, const Mouse& m){
		switch(m.button()){
		case Mouse::LEFT: printf("Pressed left mouse button.\n"); break;
		case Mouse::RIGHT: printf("Pressed right mouse button.\n"); break;
		case Mouse::MIDDLE: printf("Pressed middle mouse button.\n"); break;
		}
	}

	// This is called whenever the mouse is dragged.
	virtual void onMouseDrag(const ViewpointWindow& w, const Mouse& m){
		// Get mouse coordinates, in pixels, relative to top-left corner of window
		int x = m.x();
		int y = m.y();
		printf("Mouse dragged: %3d, %3d\n", x,y);
	}

	// *****************************************************
	// NOTE: check the App class for more callback functions

};


int main(){
	MyApp().start();
}
