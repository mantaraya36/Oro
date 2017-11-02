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

using namespace al;
using namespace std;

class Simulator :  public osc::PacketHandler {
public:
    Simulator(SharedState *state) :
        mChaos("chaos", "", 0),
        mVideoDomain(30),
#ifdef BUILDING_FOR_ALLOSPHERE
      mMaker("192.168.10.255")
#else
      mMaker("127.0.0.1")
#endif
    {
        mState = state;
        mMouseSpeed = 0;
        mMouseX = 0;
        mMouseY = 0;

        /* States */

        mNoise.resize(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ);
        mFilters.resize(GRID_SIZEX * GRID_SIZEY * GRID_SIZEZ);

        for (gam::Biquad<> &b:mFilters) {
            b.domain(mVideoDomain);
            b.freq(0.4);
        }

        for (unsigned int i = 0; i < NUM_OFRENDAS; i++) {
            mOfrendas.ofrendas[i].envelope.decay(2500.0);
//            mOfrendas.ofrendas[i].envelope.value(i/(float)NUM_OFRENDAS);
        }

        mRecvFromControl.handler(*this);
        mRecvFromControl.timeout(0.005);
        mRecvFromControl.start();
        for(auto& v : mState->wave) v = 0;
        /* States */
        mMaker.start();
    }

    ~Simulator() {
        mMaker.stop();
        mRecvFromControl.stop();
    }


    int indexAt(int x, int y, int z){
        //return (z*Nx + y)*Ny + x;
        return (y*Nx + x)*2 + z; // may give slightly faster accessing
    }

    void setMousePosition(float x, float y) {
        mMouseSpeed = 5.0f * sqrt(x * x + y * y);
        mMouseX = x;
        mMouseY = y;
    }

    void adjustChaos(float change) {
        state().chaos += change; std::cout << state().chaos << std::endl;
    }

    void reset() {
        state().chaos = 0.0;
        for (int i = 0; i < NUM_OFRENDAS; i++) {
            state().ofrendas[i] = false;
            state().posOfrendas[i].y = LAGOON_Y - 0.2;
        }
        mSenderToControl.send("/reset");
        mSenderToGraphics.send("/clear");
    }

    void onAnimate(double dt) {
        al_sec curTime = al_steady_time();
//        if (mMouseSpeed > 2) {
//            mChaos.set(mMouseSpeed/10.0f);
//            if ((int) state().interactionEnd - (int) state().interactionBegin != -1) {
//                const Vec4d &lastPoint = state().interactionPoints[state().interactionEnd];
//                if (lastPoint.x != mMouseX && lastPoint.y != mMouseY) {
//                    Vec4d newPoint(mMouseX, mMouseY, 0, mMouseSpeed);
//                    state().interactionEnd++;
//                    if (state().interactionEnd == INTERACTION_POINTS) {
//                        state().interactionEnd = 0;
//                    }
//                    state().interactionPoints[state().interactionEnd] = newPoint;
//                    state().interactionTimes[state().interactionEnd] = curTime;
//                }
//            }
//        }

//        float timeOut = 3.0;
//        for (int it = state().interactionBegin; it != state().interactionEnd; it++) {
//            if (it == INTERACTION_POINTS) {
//                it = 0;
//            }
//            if (curTime - state().interactionTimes[it] > timeOut) {
//                state().interactionBegin++;
//                if (state().interactionBegin == INTERACTION_POINTS) {
//                    state().interactionBegin = 0;
//                }
//            } else {
//                break;
//            }

//        }

        // Calculate grid deviations
//        if ()
//        for (gam::Biquad<> &b:mFilters) {
//            b.domain(mVideoDomain);
//            b.freq(0.4);
//        }

        float * dev_ = state().dev;
        unsigned int count = 0;
        for (int x = 0; x < GRID_SIZEX; x++) {
            for (int y = 0; y < GRID_SIZEY; y++) {
                for (int z = 0; z < GRID_SIZEZ; z++) {
                    if (mChaos.get() < 0.18f) {
                        *dev_++ = 0.0;
                    } else {
                        *dev_++ = mFilters[count](mNoise[count]() * (5.0f * (mChaos.get() - 0.18f)/0.82f));
                    }
                    count++;
                }
            }
        }

        // Make ofrendas come down
        for (unsigned int i = 0; i < NUM_OFRENDAS; i++) {
            if (state().ofrendas[i]) {
                float env = mOfrendas.ofrendas[i].envelope();
                if (mOfrendas.ofrendas[i].envelope.done(0.1)) {
                    state().ofrendas[i] = false;
                    //                state().posOfrendas[i].x = rnd::gaussian()* 0.9;
                } else {
//                    state().posOfrendas[i].y = env * 25.0f - 8.0f;
                    state().posOfrendas[i].y = -9.8 + LAGOON_Y +  env * 10;
                    float randomVal = rnd::uniform(-1.0, 1.0);
                    sideSpeed[i] *= 0.5;
                    sideSpeed[i] += randomVal* 0.03f;
                    state().posOfrendas[i].x += sideSpeed[i];
                    state().posOfrendas[i][3] += randomVal * 1.1f;
                }
            }
        }

        // Light movement
        if (state().chaos > 0.3) {
			state().lightPhase += 1./5600; if(state().lightPhase > 1) state().lightPhase -= 1;
		} else if (state().chaos > 0.6) {
			state().lightPhase += 1./1800; if(state().lightPhase > 1) state().lightPhase -= 1;
		}


        // Wave equation
        int zprev = 1-zcurr;
        // Compute effects of trigger

        float chaosSpeed = 0.3; //0.01

        // Activate ofrendas
        ;
        if (mDist > 0) {
            float dist = mDist;
            if (mDist > 0.1) {
                dist /=20.0;
            }
            if (mOfrendaHold++ > 2.0 * 20) {
                if(state().chaos > 0.1 && state().chaos <= 0.4 && rnd::prob(0.012)) {
                    int randOffset = rnd::uniform(NUM_OFRENDAS);
                    for (unsigned int i = 0; i < NUM_OFRENDAS; i++) {
                        int index = (randOffset + i)%NUM_OFRENDAS;
                        if (!state().ofrendas[index]) {
                            mOfrendas.ofrendas[index].envelope.reset();
                            state().posOfrendas[index].x = rnd::gaussian()* 0.3;
                            state().posOfrendas[index].y = -9.8 + LAGOON_Y;
                            state().posOfrendas[index].z =  - 4 + i/(float)NUM_OFRENDAS;
                            state().ofrendas[index] = true;
                            sideSpeed[index] = 0;
                            break;
                        }
                    }
                    mOfrendaHold = 0.0;
                }
            }
            // Activate bitcoin markers
            if(state().chaos > 0.4 && rnd::prob(0.2)){
                if (mPairHash.hashComplete()) {
//                    std::cout << mPairHash.mHash << std::endl;
//                    showBitcoinReport(mPairHash.mHash, )
                    mSenderToControl.send("/showBitcoinReport", mPairHash.mHash, mPairHash.isBitcoin());
                    mSenderToGraphics.send("/showBitcoinReport", mPairHash.mHash, mPairHash.isBitcoin());
                    mPairHash.clearHash();
                } else {
//                    std::cout << mDeltaX << "...." << mDeltaY << std::endl;
                    // Range 0.001 -> 0.04;
                    const float maxDelta = 0.03; const float minDelta = 0.00001;
                    int value1 = 16 * (abs(mDeltaX) - minDelta) / (maxDelta - minDelta);
                    int value2 = 16 * (abs(mDeltaY) - minDelta) / (maxDelta - minDelta);
                    if (value1 >= 16) value1 = 15;
                    if (value2 >= 16) value2 = 15;
                    if (value1 < 0) value1 = 0;
                    if (value2 < 0) value2 = 0;
                    std::string newPair;
                    newPair += mHexChars[value1];
                    newPair += mHexChars[value2];
                    mPairHash.nextHash(newPair, mPosX, mPosY);
                    mSenderToControl.send("/addBitcoinMarker", newPair, mPosX, mPosY);
                    mSenderToGraphics.send("/addBitcoinMarker", newPair, mPosX, mPosY);
                }
            }
            state().chaos += dist* chaosSpeed;

        }

        if(mPosX >= -50.0 && mPosY >= -50.0){
            // Add a Gaussian-shaped droplet
            //				int ix = rnd::uniform(Nx-8)+4;
            //				int iy = rnd::uniform(Ny-8)+4;
            for(int j=-4; j<=4; ++j){
                for(int i=-4; i<=4; ++i){
                    float x = mPosX*(Nx - 8) + 4;
                    float adjustedY = (mPosY* 9/16.0) + (0.5 * 5/16.0);
                    float y = Ny - adjustedY*(Ny - 8) + 4;
                    float v = 0.35*exp(-(i* i/16.0+j*j/16.0)/(0.5*0.5));
                    state().wave[indexAt(x+i, y+j, zcurr)] += v * fabs(mDist)* 20;
                    state().wave[indexAt(x+i, y+j, zprev)] += v * fabs(mDist)* 20;
                    //                            std::cout << x << "  " << y << " "<< v << std::endl;
                }
            }
            mPosX = mPosY = -100.0; // Must be last as mPosX and mPosY are used above
        }



        // Compute wave equation
        for(int j=0; j<Ny; ++j){
		for(int i=0; i<Nx; ++i){

			// Neighbor indices; wrap toroidally
			int im1 = i!=0 ? i-1 : Nx-1;
			int ip1 = i!=Nx-1 ? i+1 : 0;
			int jm1 = j!=0 ? j-1 : Ny-1;
			int jp1 = j!=Nx-1 ? j+1 : 0;

			// Get neighborhood of samples
			auto vp = state().wave[indexAt(i,j,zprev)];		// previous value
			auto vc = state().wave[indexAt(i,j,zcurr)];		// current value
			auto vl = state().wave[indexAt(im1,j,zcurr)];	// neighbor left
			auto vr = state().wave[indexAt(ip1,j,zcurr)];	// neighbor right
			auto vd = state().wave[indexAt(i,jm1,zcurr)];	// neighbor up
			auto vu = state().wave[indexAt(i,jp1,zcurr)];	// neighbor down

			// Compute next value of wave equation at (i,j)
			auto val = 2*vc - vp + velocity.get()*((vl - 2*vc + vr) + (vd - 2*vc + vu));

			// Store in previous value since we don't need it again
			state().wave[indexAt(i,j,zprev)] = val * decay.get();
		}}
        zcurr = zprev;

        state().zcurr = zcurr;
        velocity.set(0.001 + (state().chaos*state().chaos* 0.49));

        decay.set(0.93 + (state().chaos* 0.068));
//        shininess.set(50 - (state().chaos* 20));

        state().chaos = state().chaos - 0.00025;
        if (state().chaos < 0.0) {
            state().chaos = 0.0;
        }
        if (state().chaos > 1.0) {
            state().chaos = 1.0;
        }
        state().decay = decay.get();
        state().velocity = velocity.get();
        state().mouseDown = mMouseDown == 1.0;

        state().casasPhase += 0.1 + state().chaos * 0.2;
//		if (state().casasPhase > 360) { state().casasPhase -= 360;}

        mMaker.set(state());
        mChaos.set(state().chaos);

        // Send to Control
        mSenderToControl.send("/chaos", state().chaos);
        mSenderToControl.send("/decay", decay.get());
        mSenderToControl.send("/velocity", velocity.get());

        // Send to Audio
        mSenderToAudio.send("/chaos", state().chaos);
        mSenderToAudio2.send("/chaos", state().chaos);

        mDist = 0;
    }

    virtual void onMessage(osc::Message &m) override {
//        m.print();
        if (m.addressPattern() == "/dist" && m.typeTags() == "fff") {
            m >> mDist >> mDeltaX >> mDeltaY;
        } else if (m.addressPattern() == "/drop" && m.typeTags() == "ff") {
            m >> mPosX >> mPosY;
        } else if (m.addressPattern() == "/mouseDown" && m.typeTags() == "f") {
            m >> mMouseDown;
            mSenderToAudio.send("/mouseDown", mMouseDown);
            mSenderToAudio2.send("/mouseDown", mMouseDown);
//            std::cout << mMouseDown << std::endl;
        }
    }

    Parameter mChaos;

//	Parameter mSpeedX, mSpeedY, mSpeedZ;
    /* Simulation states and data */
    SharedState *mState; // To renderers

    Parameter decay {"decay", "", 0.96, "", 0.8, 0.999999};	// Decay factor of waves, in (0, 1]
    Parameter velocity{"velocity", "", 0.4999999, "", 0, 0.4999999};	// Velocity of wave propagation, in (0, 0.5]

    // Ofrenda
    int mOfrendaHold = 0;

    // Bitcoin
    std::string mHexChars {"0123456789ABCDEF"};
    PairHash mPairHash;

    // From controllers
    float mMouseSpeed;
    float mMouseX, mMouseY;
    float mPosX {0};
    float mPosY {0};
    float mDeltaX {0};
    float mDeltaY {0};
    float mDist {0};
    float mMouseDown;

    // For onAnimate computation
    gam::Domain mVideoDomain; // Simulation frame rate
    Ofrenda_Data mOfrendas;
    std::vector<gam::Biquad<> > mFilters;
    std::vector<gam::NoiseBrown<> > mNoise;

    float sideSpeed[NUM_OFRENDAS];

    cuttlebone::Maker<SharedState> mMaker;

    int zcurr=0;		// The current "plane" coordinate representing time

    SharedState &state() {return *mState;}

    osc::Send mSenderToAudio {AUDIO_IN_PORT, AUDIO_IP_ADDRESS};
    osc::Send mSenderToAudio2 {AUDIO2_IN_PORT, AUDIO2_IP_ADDRESS};
    osc::Send mSenderToControl {CONTROL_IN_PORT, CONTROL_IP_ADDRESS};
    osc::Send mSenderToGraphics {GRAPHICS_IN_PORT, GRAPHICS_IP_ADDRESS};
    osc::Recv mRecvFromControl {SIMULATOR_IN_PORT};
};


class MyApp : public App {
public:

    SharedState mState;
    SharedPainter mPainter;
    Simulator mSimulator;

    SharedState& state() {return mState;}

	ShaderProgram mShader;

	// This constructor is where we initialize the application
	MyApp(): mPainter(&mState, &mShader, GRAPHICS_IN_PORT, 12098),
        mSimulator(&mState)
	{
//		mSpeedX = mSpeedY = 0;
//        omni().resolution(256);
		// Configure the camera lens
//		lens().near(0.1).far(25).fovy(45);

		// Set navigation position and orientation

		initWindow(Window::Dim(0,0, 600,400), "Simulator", 20);

        mPainter.setTreeMaster();
		std::cout << "Constructor done" << std::endl;
	}

    void setup() {
        mPainter.onInit();
    }

	virtual void onAnimate(double dt) override {
//        mTaker.get(state());

        static bool first_frame {true};
        if (first_frame) {
          setup();
          first_frame = false;
        }
        mSimulator.onAnimate(dt); // State could be shared here when simulator is local
//        state().nav = nav();

        // Update wave equation
        for(int j=0; j<Ny; ++j){
            for(int i=0; i<Nx; ++i){
                int idx = j*Nx + i;
                mPainter.waterMesh.vertices()[idx].z = mSimulator.state().wave[indexAt(i,j, mSimulator.zcurr)] / mSimulator.decay.get();
            }
        }

        mPainter.waterMesh.generateNormals();
	}

	virtual void onDraw(Graphics& g) override {
        mPainter.onDraw(g);
	}


	// This is called whenever a key is pressed.
	virtual void onKeyDown(const Keyboard& k) override {

		// Use a switch to do something when a particular key is pressed
		switch(k.key()){

		// For printable keys, we just use its character symbol:
        case 't': printf("Pressed t.\n"); mSimulator.adjustChaos(0.03);  break;
		case 'y': printf("Pressed y.\n"); mSimulator.adjustChaos(-0.03); break;
		case 'n': printf("Pressed n.\n"); break;
		case '.': printf("Pressed period.\n"); break;
        case 'r': mSimulator.reset();

		// For non-printable keys, we have to use the enums described in the
		// Keyboard class:
		case Keyboard::RETURN: printf("Pressed return.\n"); break;
		case Keyboard::DELETE: printf("Pressed delete.\n"); break;
		case Keyboard::F1: printf("Pressed F1.\n"); break;
		}
        return;
	}
};


int main(){
	MyApp().start();
}
