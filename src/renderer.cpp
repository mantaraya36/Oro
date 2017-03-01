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

#include "Cuttlebone/Cuttlebone.hpp"

#include "allocore/graphics/al_Shapes.hpp"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/math/al_Random.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "alloGLV/al_ParameterGUI.hpp"
#include "alloutil/al_OmniApp.hpp"

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"

#include "common.hpp"

//#define SURROUND
using namespace al;
using namespace std;


class MyApp : public OmniApp {
public:

    SharedState mState;
    SharedPainter mPainter;

    cuttlebone::Taker<SharedState> mTaker;
    SharedState& state() {return mState;}

    float mMouseX, mMouseY;
    float mSpeedX, mSpeedY;

	ShaderProgram mShader;

    // Audio
	Granulator granX, granY, granZ;
	Granulator background1;
	Granulator background2;
	Granulator background3;
    gam::SineR<float> fluctuation1, fluctuation2;

    // Parameters
	Parameter gainBackground1 {"background1", "", 0.2f, "", 0.0f, 1.0f};
	Parameter gainBackground2 {"background2", "", 0.2f, "", 0.0f, 1.0f};
	Parameter gainBackground3 {"background3", "", 0.2f, "", 0.0f, 1.0f};

	ParameterGUI mParameterGUI;


	// This constructor is where we initialize the application
	MyApp(): mPainter(&mState, &shader()),
	    granX("Bounced Files/Piezas oro 1.wav"),
	    granY("Bounced Files/Piezas oro 2.wav"),
	    granZ("Bounced Files/Piezas oro 2.wav"),
	    background1("Bounced Files/Bajo agua.wav"),
	    background2("Bounced Files/Bajo agua.wav"),
	    background3("Bounced Files/Bajo agua.wav")

	{
		AudioDevice::printAll();
//        omni().resolution(256);
		// Configure the camera lens
//		lens().near(0.1).far(25).fovy(45);

		// Set navigation position and orientation

		initWindow(Window::Dim(0,0, 600,400), "Untitled", 30);

        // Audio

        fluctuation1.freq(0.2f);
		fluctuation2.freq(0.3f);
#ifdef SURROUND
		audioIO().device(5);
		initAudio(48000, 256, 8, 0);
#else
		audioIO().device(0);
//		initAudio("default", 48000, 64, 0, 2);
#endif

//		mParameterGUI.setParentApp(this);
//		mParameterGUI << new glv::Label("Parameter GUI example");
//		mParameterGUI << gainBackground1 << gainBackground2 << gainBackground3;
        mTaker.start();
		std::cout << "Constructor done" << std::endl;
	}

	virtual bool onCreate() override {
//		mShader.compile(fogVert, fogFrag);
        OmniApp::onCreate();

        nav().pos().set(0,0,10);
//		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);
        omni().clearColor() = Color(0.02, 0.02, 0.04, 0.0);

        return true;
	}

    void setup() {

        mPainter.onInit();

        auto& s = shader();
        s.begin();
        // the shader will look for texture0 uniform's data
        // at binding point '0' >> we will bind the texture at '0'
        s.uniform("texture0", 0);
        // how much we mix the texture
        s.uniform("texture", 1.0);
        s.uniform("lighting", 0.9);
        s.end();
    }

	virtual void onAnimate(double dt) override {
        mTaker.get(state());

        static bool first_frame {true};
        if (first_frame) {
          setup();
          first_frame = false;
        }
//        state().nav = nav();
	}

	virtual void onDraw(Graphics& g) override {

        mPainter.onDraw(g);
	}


    virtual void onSound(AudioIOData& io) override {

		// Things here occur at block rate...

		float mouseSpeedScale = 50.0f;
		while(io()){
			//float in = io.in(0);
			float fluct1 = fluctuation1();
			float fluct2 = fluctuation2();

			float out1 = granX() * mSpeedX * mouseSpeedScale;
			float out2 = granY() * mSpeedY * mouseSpeedScale;

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

	// This is called whenever a key is pressed.
//	virtual bool onKeyDown(const Keyboard& k) override {

//		// Use a switch to do something when a particular key is pressed
//		switch(k.key()){

//		// For printable keys, we just use its character symbol:
//		case '1': printf("Pressed 1.\n"); break;
//		case 'y': printf("Pressed y.\n"); break;
//		case 'n': printf("Pressed n.\n"); break;
//		case '.': printf("Pressed period.\n"); break;
//		case ' ': printf("Pressed space bar.\n");
//			mSimulator.addChaos();
//			break;

//		// For non-printable keys, we have to use the enums described in the
//		// Keyboard class:
//		case Keyboard::RETURN: printf("Pressed return.\n"); break;
//		case Keyboard::DELETE: printf("Pressed delete.\n"); break;
//		case Keyboard::F1: printf("Pressed F1.\n"); break;
//		}
//        return true;
//	}

//	// This is called whenever a mouse button is pressed.
//	virtual bool onMouseDown(const Mouse& m) override {
//		switch(m.button()){
//		case Mouse::LEFT: printf("Pressed left mouse button.\n"); break;
//		case Mouse::RIGHT: printf("Pressed right mouse button.\n"); break;
//		case Mouse::MIDDLE: printf("Pressed middle mouse button.\n"); break;
//		}
//        return true;
//	}

//	// This is called whenever the mouse is dragged.
//	virtual bool onMouseDrag(const Mouse& m) override {
//		// Get mouse coordinates, in pixels, relative to top-left corner of window
//		int x = m.x();
//		int y = m.y();
//		printf("Mouse dragged: %3d, %3d\n", x,y);
//        return true;
//	}

//    virtual bool onMouseMove(const Mouse &m) override {
//        std::cout << "mousr" << m.x() << "-" << width() << std::endl;
//        mMouseX = (m.x() - (float)width()/2)/(float)width();
//        mMouseY = -(m.y() - (float)height()/2)/(float)height();

//        mSpeedX = m.dx()/(float)width();
//        mSpeedY = m.dy()/(float)height();

//        mSimulator.setMousePosition(mMouseX, mMouseY);
//        return true;
//    }

    virtual std::string vertexCode() override;
    virtual std::string fragmentCode() override;


	// *****************************************************
	// NOTE: check the App class for more callback functions

};


inline std::string MyApp::vertexCode() {
  return vertexShader;
}

inline std::string MyApp::fragmentCode() {
  return fragmentShader;
}


int main(){
	MyApp().start();
}
