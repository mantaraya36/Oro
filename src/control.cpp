/*
Allocore Example: Wave Equation

Description:
This implements a discretized version of the wave equation:

	u(r, t+1) = 2u(r,t) - u(r,t-1) + v^2 [u(r+1, t) - 2u(r,t) + u(r-1,t)]

where

	u	is the wave equation,
	r	is position,
	t	is time, and
	v	is the velocity or CFL (Courant-Friedrichs-Lewy) number.

The simulation adds random Gaussian-shaped drops to simulate water droplets
falling into a pool. A minor artifact is increased rippling along the wavefronts
in the x and y directions.

See also: http://locklessinc.com/articles/wave_eqn/

Author:
Lance Putnam, Oct. 2014
*/

#include "allocore/io/al_App.hpp"
#include "allocore/ui/al_Parameter.hpp"

#include "common.hpp"
#include "render_tree.hpp"
using namespace al;


class MyApp : public App, public osc::PacketHandler {
public:

	MyApp() {

		// Add a tessellated plane
		addSurface(mesh, Nx,Ny);
//		mesh.color(HSV(0.6, 0.2, 0.9));
        mesh.color(0.1, 0.4, 0.1);

		nav().pullBack(2.1);

		initWindow();
//        navControl().useMouse(false);
        window().remove(navControl());
        reset();
        for(auto& v : wave) v = 0;
        mOSCReceiver.handler(*this);
        mOSCReceiver.timeout(0.005);
        mOSCReceiver.start();
	}

    void reset() {
        mIntroTextModule = mRenderTree.createModule<TextRenderModule>();
        mIntroTextModule->loadFont("fonts/CaslonAntique.ttf", 32, true);
        mIntroTextModule->setScale(0.4);
        mIntroTextModule->setText("In sacred lagoons in the Colombian Andes,");
        mIntroTextModule->setPosition(Vec3d(-0.6, 0.08, 0.04999));

        mIntroTextModule2 = mRenderTree.createModule<TextRenderModule>();
        mIntroTextModule2->loadFont("fonts/CaslonAntique.ttf", 32, true);
        mIntroTextModule2->setScale(0.4);
        mIntroTextModule2->setText("Gold was thrown in as part of a ritual");
        mIntroTextModule2->setPosition(Vec3d(-0.6, -0.0, 0.04999));
        mIntroTextFading = false;
    }

    void showBitcoinReport(std::string hash, bool isBitcoin) {

        auto module = mRenderTree.createModule<TextRenderModule>();
        module->setFontSize(18);
        module->setScale(0.25);
        module->setText(hash);
        module->setPosition(Vec3d(-0.48, 0.45, 0.079990));

        module->addBehavior(std::make_shared<Sink>(5* window().fps(), -0.08));
        module->addBehavior(std::make_shared<FadeOut>(6* window().fps(),3* window().fps()));

        auto module2 = mRenderTree.createModule<TextRenderModule>();
        module2->setFontSize(18);
        module2->setScale(0.2);
        if (isBitcoin) {
            module2->setText("Bitcoin!!");
        } else {
            module2->setText("Not a Bitcoin");
        }
        module2->setPosition(Vec3d(-0.4, 0.4, 0.079990));
//                    module2->addBehavior(std::make_shared<FadeIn>(2* window().fps()));
        module2->addBehavior(std::make_shared<Sink>(4* window().fps(), -0.08));
        module2->addBehavior(std::make_shared<FadeOut>(4* window().fps(),5* window().fps()));

//        auto lineStrip = mRenderTree.createModule<LineStripModule>();
//        for (int i = 0; i < 32; i++) {
//            lineStrip->addVertex(mPairHash.mHashX[i] * window(0).width()/2, mPairHash.mHashY[i] * window(0).height()/2, 0.1);
//        }
//        lineStrip->addBehavior(std::make_shared<FadeOut>(7* window().fps(),3* window().fps()));
    }

    void addBitcoinMarker(std::string text, float x, float y) {
        auto module = mRenderTree.createModule<TextRenderModule>();
        module->setFontSize(24);
        module->setScale(0.3);
        module->setText(text);
        module->setPosition(Vec3d(x- 0.5, y- 0.5, 0.2999));
        module->addBehavior(std::make_shared<Timeout>(10 * window().fps()));
        module->addBehavior(std::make_shared<Sink>(3* window().fps(), -0.3));
        module->addBehavior(std::make_shared<FadeOut>(7* window().fps(),3* window().fps()));
    }

    void onAnimate(double dt){

        int zprev = 1-zcurr;
        if (mDown) {
            for(int k=0; k<3; ++k){
                if(rnd::prob(0.3)){
                    mOSCSender.send("/drop", mPosX, mPosY);
                    // Add a Gaussian-shaped droplet
                    //				int ix = rnd::uniform(Nx-8)+4;
                    //				int iy = rnd::uniform(Ny-8)+4;
                    for(int j=-4; j<=4; ++j){
                        for(int i=-4; i<=4; ++i){
                            float x = mPosX*(Nx - 8) + 4;
                            float adjustedY = (mPosY* 9/16.0) + (0.5 * 5/16.0);
                            float y = adjustedY*(Ny - 8) + 4;
                            float v = 0.35*exp(-(i* i/16.0+j*j/16.0)/(0.5*0.5));
                            wave[indexAt(x+i, y+j, zcurr)] += v;
                            wave[indexAt(x+i, y+j, zprev)] += v;
//                            std::cout << x << "  " << y << " "<< v << std::endl;
                        }
                    }
                }
            }
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
                auto vp = wave[indexAt(i,j,zprev)];		// previous value
                auto vc = wave[indexAt(i,j,zcurr)];		// current value
                auto vl = wave[indexAt(im1,j,zcurr)];	// neighbor left
                auto vr = wave[indexAt(ip1,j,zcurr)];	// neighbor right
                auto vd = wave[indexAt(i,jm1,zcurr)];	// neighbor up
                auto vu = wave[indexAt(i,jp1,zcurr)];	// neighbor down

                // Compute next value of wave equation at (i,j)
                auto val = 2*vc - vp + velocity.get()*((vl - 2*vc + vr) + (vd - 2*vc + vu));

                // Store in previous value since we don't need it again
                wave[indexAt(i,j,zprev)] = val * decay.get();
            }}
        zcurr = zprev;

        float chaos = mChaos;

        if(chaos < 0.3f) {
            float localFactor =  chaos/0.3f;
            color.set( Vec3f(localFactor * 1.0f, localFactor * 0.9f, localFactor * 0.2f));
        } else if(chaos < 0.6f) {
            float localFactor =  (chaos- 0.3f)/0.3f;
            color.set( Vec3f(1.0f, 0.9f, 0.2f + (localFactor * 0.8f)));
        } else {
            float localFactor =  (chaos- 0.6f)/0.3f;
            color.set( Vec3f(1.0f, (0.9f - localFactor), (1.0f - localFactor)));
        }

        shininess.set(50 - (chaos* 20));

        // Update wave equation
        for(int j=0; j<Ny; ++j){
            for(int i=0; i<Nx; ++i){
                int idx = j*Nx + i;
                mesh.vertices()[idx].z = wave[indexAt(i,j,zprev)] / decay.get();
            }}

        mesh.generateNormals();

    }

    void onDraw(Graphics& g){
        g.pushMatrix();
        mtrl.specular(RGB(0));
        mtrl.shininess(shininess.get());
        mtrl();
        light.dir(1,1,1);
        light();
        mesh.colors()[0] = color.get();
        mesh.colors()[0] .a = 1.0;
        g.draw(mesh);
        g.popMatrix();
        mRenderTree.render(g);
	}

    virtual void onMouseDown(const Mouse &m) override {
        mDown = m.down(0);
        mOSCSender.send("/mouseDown", 1.0f);
    }
    virtual void onMouseUp(const Mouse &m) override  {
        mDown = m.down(0);
        mOSCSender.send("/mouseDown", 0.0f);
    }

    virtual void onMouseDrag(const Mouse &m) override {
        float oldX = mPosX;
        float oldY = mPosY;
        mPosX = m.x()/(float)window(0).width();
        if (mPosX > 1.0) mPosX = 1.0;
        mPosY = 1.0 - m.y()/(float)window(0).height();
        if (mPosY > 1.0) mPosY = 1.0;
//        std::cout << mPosX << std::endl;
        mDist = sqrt((oldX - mPosX)* (oldX - mPosX) + (oldY - mPosY)*(oldY - mPosY));
        mDeltaX = mPosX - oldX;
        mDeltaY = mPosY - oldY;
        mOSCSender.send("/dist", mDist, mDeltaX, mDeltaY);
        if (mIntroTextModule) {
            if (mIntroTextModule->done()) {
                mIntroTextModule = nullptr;
            } else if (!mIntroTextFading) {
                mIntroTextModule->addBehavior(std::make_shared<FadeOut>(15* window().fps()));
                mIntroTextModule->addBehavior(std::make_shared<Sink>(15* window().fps(), -0.05));
                mIntroTextModule2->addBehavior(std::make_shared<FadeOut>(20* window().fps()));
                mIntroTextModule2->addBehavior(std::make_shared<Sink>(20* window().fps(), -0.05));
                mIntroTextFading = true;
            }
        }
        if (mIntroTextModule2) {
            if (mIntroTextModule2->done()) {
                mIntroTextModule2 = nullptr;
            }
        }
    }

    virtual void onKeyDown(const Keyboard &k) override {
        if (k.key() == 'r') {
            reset();
        }
    }

    virtual void onMessage(osc::Message &m) override {
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
        } else if (m.addressPattern() == "/reset") {
            reset();
        } else if (m.addressPattern() == "/chaos" && m.typeTags() == "f") {
            m >> mChaos;
        } else if (m.addressPattern() == "/velocity" && m.typeTags() == "f") {
            float val;
            m >> val;
            velocity.set(val);
        } else if (m.addressPattern() == "/decay" && m.typeTags() == "f") {
            float val;
            m >> val;
            decay.set(val);
        }
    }

    Mesh mesh;
    Light light;
    Material mtrl;

    float wave[Nx*Ny*2];
    int zcurr=0;		// The current "plane" coordinate representing time

    float mPosX {0};
    float mPosY {0};
    float mDeltaX {0};
    float mDeltaY {0};
    bool mDown {false};
    float mDist {0.0};
    osc::Send mOSCSender {SIMULATOR_IN_PORT, SIMULATOR_IP_ADDRESS};
    osc::Recv mOSCReceiver {CONTROL_IN_PORT};
    RenderTree mRenderTree;
    std::shared_ptr<TextRenderModule> mIntroTextModule;
    std::shared_ptr<TextRenderModule> mIntroTextModule2;
    bool mIntroTextFading {false};

    // Remote parameters
    float mChaos;

    // Local parameters
    ParameterVec3 color{"color", "", Vec3f(0.1, 0.4, 0.1)};
    Parameter shininess{"shininess", "", 30, "", 0, 50};
    Parameter decay {"decay", "", 0.96, "", 0.8, 0.999999};	// Decay factor of waves, in (0, 1]
    Parameter velocity{"velocity", "", 0.4999999, "", 0, 0.4999999};	// Velocity of wave propagation, in (0, 0.5]

};

int main(){
	MyApp().start();
}
