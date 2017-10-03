#include "alloutil/al_OmniApp.hpp"
#include "alloutil/al_OmniStereoGraphicsRenderer.hpp"

using namespace al;
using namespace std;




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
// #define BUILDING_FOR_ALLOSPHERE

struct Renderer : public OmniStereoGraphicsRenderer {
public:
    SharedState mState;
    SharedPainter mPainter;

    SharedState& state() {return mState;}

    cuttlebone::Taker<SharedState> mTaker;
//    float mMouseX, mMouseY;
//    float mSpeedX, mSpeedY;
    // From control interface
    float mPosX {0};
    float mPosY {0};
    float mDeltaX {0};
    float mDeltaY {0};

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
	Renderer(): mPainter(&mState, &shader(), GRAPHICS_IN_PORT, GRAPHICS_SLAVE_PORT),
	    granX("Bounced Files/Piezas oro 1.wav"),
	    granY("Bounced Files/Piezas oro 2.wav"),
	    granZ("Bounced Files/Piezas oro 2.wav"),
	    background1("Bounced Files/Modal.csd-000.wav"),
	    background2("Bounced Files/Modal.csd-000.wav"),
	    background3("Bounced Files/Bajo agua.wav")

	{
		AudioDevice::printAll();

        fluctuation1.freq(0.2f);
		fluctuation2.freq(0.3f);

//		mParameterGUI.setParentApp(this);
//		mParameterGUI << new glv::Label("Parameter GUI example");
//		mParameterGUI << gainBackground1 << gainBackground2 << gainBackground3;

//        mPainter.setTreeMaster();
        std::shared_ptr<TextRenderModule> module = mPainter.mRenderTree.createModule<TextRenderModule>();
        module->setText("Hello");
        module->setPosition(Vec3f(0, 0, -1));
		std::cout << "Constructor done" << std::endl;
	}

	virtual bool onCreate() override {
//		mShader.compile(fogVert, fogFrag);
        OmniStereoGraphicsRenderer::onCreate();

        nav().pos().set(0,3,10);
//		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);
        omni().clearColor() = Color(0.02, 0.02, 0.04, 0.0);

        return true;
	}

    ~Renderer() {
        mTaker.stop();
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

    virtual void start() {
        mTaker.start();                        // non-blocking
        OmniStereoGraphicsRenderer::start();  // blocks
    }

	virtual void onAnimate(double dt) override {
        int got = mTaker.get(state());
//        if (got > 0) {
//            std::cout << "got " << got << std::endl;
//        }
        
        static bool first_frame = true;
        if (first_frame) {
          setup();
          first_frame = false;
        }
        // Update wave equation
        for(int j=0; j<Ny; ++j){
            for(int i=0; i<Nx; ++i){
                int idx = j*Nx + i;
                mPainter.waterMesh.vertices()[idx].z = state().wave[indexAt(i,j, state().zcurr)] / state().decay;
            }
        }

        mPainter.waterMesh.generateNormals();
	}

	virtual void onDraw(Graphics& g) override {
        mPainter.onDraw(g);
	}

//    virtual void onDrawOmni(OmniStereo &om) override {
//        mPainter.onDraw(om.graphics() );
//	}

    virtual std::string vertexCode() override {
        return vertexShader;
    }
    virtual std::string fragmentCode() override {
        return fragmentShader;
    }
};


int main() {
  Renderer().start();
}
