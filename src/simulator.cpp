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

#include "Cuttlebone/Cuttlebone.hpp"

#include "common.hpp"

//#define SURROUND
using namespace al;
using namespace std;


class MyApp : public OmniApp {
public:

    SharedState mState;
    cuttlebone::Maker<SharedState> mMaker;
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
	MyApp(): mPainter(&mState),
        mVideoDomain(30),
	    mChaos("chaos", "", 0),
	    mSpeedX("speedX", "", 0),
	    mSpeedY("speedY", "", 0),
	    mSpeedZ("speedZ", "", 0),
	    granX("Bounced Files/Piezas oro 1.wav"),
	    granY("Bounced Files/Piezas oro 2.wav"),
	    granZ("Bounced Files/Piezas oro 2.wav"),
	    background1("Bounced Files/Bajo agua.wav"),
	    background2("Bounced Files/Bajo agua.wav"),
	    background3("Bounced Files/Bajo agua.wav")

	{
		AudioDevice::printAll();

//        omni().resolution(256);

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

        // Configure the camera lens
//		lens().near(0.1).far(25).fovy(45);
        lens().far(35).near(0.01);
	}


	virtual bool onCreate() override {
        OmniApp::onCreate();


        mPainter.onCreate();

        auto& s = shader();
        s.begin();
        // the shader will look for texture0 uniform's data
        // at binding point '0' >> we will bind the texture at '0'
        s.uniform("texture0", 0);
        // how much we mix the texture
        s.uniform("texture", 1.0);
        s.end();

        nav().pos().set(0,0,4);
        omni().clearColor() = Color(0, 0.4, 0.4, 1);
        return true;

        ////////////////////////////////////
        mShader.compile(fogVert, fogFrag);

//		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);
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
        mMaker.set(state());
//        state().nav = nav();
	}


	virtual void onDraw(Graphics& g) override {
//        nav().pos() = state().nav;
//		g.fog(lens().far(), lens().near()+1, omni().clearColor());

        mPainter.onDraw(g);

//        mShader.begin();
//		mShader.uniform("fog_color", 0.8, 0.8, 0.8);
//		mShader.end();
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
