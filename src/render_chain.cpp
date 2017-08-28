
#include "allocore/io/al_App.hpp"
#include "render_tree.hpp"

using namespace al;


class MyApp: public App, public osc::PacketHandler {
public:
    MyApp() {}

    virtual void onCreate(const ViewpointWindow& win) override
    {
        // Configure the camera lens
		lens().near(0.1).far(25).fovy(45);

		// Set navigation position and orientation
		nav().pos(0,0,4);
		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);
        // Set background color (default is black)
		background(HSV(0.5, 1, 0.5));


        std::shared_ptr<LineStripModule> lineStrip = mRenderTree.createModule<LineStripModule>();
        lineStrip->setDelta(0.01);
        for (int i = 0; i < 100; i++) {
            lineStrip->addValue(sin(i/10.));
        }
        lineStrip->setPosition(Vec3d(0, 0, -4));
        mModules.push_back(lineStrip);


        std::shared_ptr<TextRenderModule> textRender = mRenderTree.createModule<TextRenderModule>();
        textRender->setPosition(Vec3d(0, 0, +0.1));
        mModules.push_back(textRender);


        std::shared_ptr<ImageRenderModule> image = std::make_shared<ImageRenderModule>();
        image->loadImage("Fotos/MO_04alpha.png");
        lineStrip->addChild(image);
//        mChain.addModule(image);
        mModules.push_back(image);

        mOSCRecv.handler(*this);
        mOSCRecv.timeout(0.1); // set receiver to block with timeout
        mOSCRecv.start();

    }

    virtual void onAnimate(double dt) override
    {
        pos.x = pos.x + 0.01;
        pos.z = -4;
        if (pos.x > 1.5) { pos.x = -1.5; }
        mModules[0]->setPosition(pos);
    }

    virtual void onDraw(Graphics& g) override
    {
        mRenderTree.render(g);
    }

    virtual void onMessage(osc::Message &m) override
    {
        m.print();
        mTreeHandler.consumeMessage(m, "");
    }

private:
    RenderTree mRenderTree;
    RenderTreeHandler mTreeHandler{mRenderTree};
    std::vector<std::shared_ptr<RenderModule>> mModules;

    //////////////
    Vec3d pos;
    float temp;

};



int main(int argc, char *argv[]) {
    MyApp app;
    app.initWindow(Window::Dim(0,0, 600,400), "Simple App");
    app.start();
    return 0;
}
