#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#include "allocore/al_Allocore.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "allocore/ui/al_Preset.hpp"
#include "allocore/ui/al_ParameterMIDI.hpp"
#include "allocore/ui/al_PresetMIDI.hpp"

#include "alloGLV/al_ControlGLV.hpp"
#include "alloGLV/al_ParameterGUI.hpp"
#include "alloGLV/al_SequencerGUI.hpp"

#include "chaos_synth.hpp"

using namespace std;
using namespace al;

#define SURROUND

#define SYNTH_POLYPHONY 1


class ChaosSynthApp: public App
{
public:
    ChaosSynthApp(int midiChannel) : mMidiChannel(midiChannel - 1)
    {
    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    void initializeValues();
    void initializeGui();
    void initializePresets();

    virtual void onCreate(const ViewpointWindow& win) override {
        gui.setParentWindow(*windows()[0]);

        controlView.parentWindow(*windows()[0]);
        windows()[0]->remove(navControl());
    }

    virtual void onSound(AudioIOData &io) override;

    virtual void onKeyDown(const Keyboard& k) override;
    virtual void onKeyUp(const Keyboard& k) override;

    //
//    void multiplyAmps(float factor);
//    void randomizeAmps(float max, bool sortPartials = true);

//    //
//    void multiplyWidths(float factor);
//    void randomizeWidths(float max, bool sortPartials = true);

//    // Transform partials
//    void multiplyFactors(float factor);
//    void randomizeFactors(float max, bool sortPartials = true);

    void randomizeNoisy();
    void randomizeClean();

    // Envelope
    void trigger(int id);
    void release(int id);

    void triggerKey(int id);
    void releaseKey(int id);

    vector<int> outputRouting;
private:

    // Parameters
    Parameter mFundamental {"Fundamental", "", 55.0, "", 0.0, 9000.0};

    Parameter keyboardOffset {"Key Offset", "", 0.0, "", -20, 40};


    //Sequencing
//    PresetSequencer sequencer;
//    SequenceRecorder recorder;

    // GUI
    ParameterGUI gui;

    glv::DropDown viewSelector;
    glv::Box controlsBox {glv::Direction::S};
    glv::Box widthsBox {glv::Direction::S};
    glv::Box ampsBox {glv::Direction::S};
    GLVDetachable controlView;

    // MIDI Control
    PresetMIDI presetMIDI;
    ParameterMIDI parameterMIDI;
    int mMidiChannel;

    ParameterBool midiLight{"MIDI", "", false};

    MIDIIn midiIn {"USB Oxygen 49"};


    static void midiCallback(double deltaTime, std::vector<unsigned char> *msg, void *userData){
        ChaosSynthApp *app = static_cast<ChaosSynthApp *>(userData);
        unsigned numBytes = msg->size();
        app->midiLight.set(true);
//        midiLightWaiter(app);

        if(numBytes > 0){
            unsigned char status = msg->at(0);
            if(MIDIByte::isChannelMessage(status)){
                unsigned char type = status & MIDIByte::MESSAGE_MASK;
                unsigned char chan = status & MIDIByte::CHANNEL_MASK;
                if ((int) chan == app->mMidiChannel) {
                    switch(type){
                    case MIDIByte::NOTE_ON:
                        //                    printf("Note %u, Vel %u", msg->at(1), msg->at(2));
                        if (msg->at(2) != 0) {
                            app->mFundamental.set(midi2cps(msg->at(1)));
                            app->trigger(msg->at(1));
                        } else {
                            //                        app->release(msg->at(1));
                        }
                        break;

                    case MIDIByte::NOTE_OFF:

                        //                    app->release(msg->at(1));
                        //                    printf("Note %u, Vel %u", msg->at(1), msg->at(2));
                        break;

                    case MIDIByte::PITCH_BEND:
                        //                    printf("Value %u", MIDIByte::convertPitchBend(msg->at(1), msg->at(2)));
                        break;
                    case MIDIByte::CONTROL_CHANGE:
                        //                    printf("%s ", MIDIByte::controlNumberString(msg->at(1)));
                        //                    switch(msg->at(1)){
                        //                    case MIDIByte::MODULATION:
                        //                        printf("%u", msg->at(2));
                        //                        break;
                        //                    }
                        break;
                    case MIDIByte::PROGRAM_CHANGE:
                        app->synth[0].mPresetHandler.recallPreset(msg->at(1));
                        break;
                    default:;
                    }
                }
            }
        }
    }

    // Synthesis
    ChaosSynth synth[SYNTH_POLYPHONY];
};

void ChaosSynthApp::initializeValues()
{
}

void ChaosSynthApp::initializeGui()
{
    gui << SequencerGUI::makePresetHandlerView(synth[0].mPresetHandler, 1.0, 12, 4);
    gui << synth[0].mLevel; // << mFundamental;
// //   gui << mCumulativeDelay << mCumulativeDelayRandomness;
//    gui << mArcStart << mArcSpan;
//	gui << mAttackCurve << mReleaseCurve;
    gui << midiLight;
    gui << keyboardOffset;

    glv::Button *randomizeButton = new glv::Button;
    randomizeButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ChaosSynthApp *app = n.receiver<ChaosSynthApp>();
        if (b->getValue() == 1) {
            app->randomizeNoisy();
        }
    }, glv::Update::Value, this);
    randomizeButton->property(glv::Momentary, true);
    controlsBox << randomizeButton << new glv::Label("Randomize Noisy");

    glv::Button *randomize2Button = new glv::Button;
    randomize2Button->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ChaosSynthApp *app = n.receiver<ChaosSynthApp>();
        if (b->getValue() == 1) {
            app->randomizeClean();
        }
    }, glv::Update::Value, this);
    randomize2Button->property(glv::Momentary, true);
    controlsBox << randomize2Button << new glv::Label("Randomize Clear");

    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        controlsBox << ParameterGUI::makeParameterView(synth[i].envFreq1);
        controlsBox << ParameterGUI::makeParameterView(synth[i].envFreq2);
        controlsBox << ParameterGUI::makeParameterView(synth[i].phsrFreq1);
        controlsBox << ParameterGUI::makeParameterView(synth[i].phsrFreq2);
        controlsBox << ParameterGUI::makeParameterView(synth[i].noiseRnd);
        controlsBox << ParameterGUI::makeParameterView(synth[i].changeProb);
        controlsBox << ParameterGUI::makeParameterView(synth[i].changeDev);
    }
    controlsBox.fit();
    controlsBox.enable(glv::DrawBack);
    controlsBox.anchor(glv::Place::TR);
    controlsBox.set(-360, 30, controlsBox.width(), controlsBox.height());
    controlView << controlsBox;

    controlView.set(300, 0, 500, 400);

}

void ChaosSynthApp::initializePresets()
{

    // MIDI Control of parameters
    unsigned int midiControllerPort = 0;
    parameterMIDI.init(midiControllerPort);
//    parameterMIDI.connectControl(mCumulativeDelay, 75, 1);
//    parameterMIDI.connectControl(mCumulativeDelayRandomness, 76, 1);

    // MIDI control of presets
    // 74 71 91 93 73 72 5 84 7
    // 75 76 77 78 74 71 24 102
    presetMIDI.init(midiControllerPort, synth[0].mPresetHandler);
    presetMIDI.setMorphControl(102, 1, 0.0, 8.0);
    // MIDI preset mapping
    presetMIDI.connectProgramToPreset(1, 0, 36, 24, 59);
    unsigned portToOpen = 0;
    // Print out names of available input ports
    for(unsigned i=0; i< midiIn.getPortCount(); ++i){
        printf("Port %u: %s\n", i, midiIn.getPortName(i).c_str());
    }
    try {
        // Open the port specified above
        midiIn.openPort(portToOpen);
    }
    catch(MIDIError &error){
        error.printMessage();
    }

    midiIn.setCallback(ChaosSynthApp::midiCallback, this);
}

void ChaosSynthApp::onSound(AudioIOData &io)
{
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        if (!synth[i].done()) {
            synth[i].generateAudio(io);
            io.frame(0);
        }
    }
}

void ChaosSynthApp::onKeyDown(const Keyboard &k)
{
    triggerKey(k.key());
}


void ChaosSynthApp::onKeyUp(const Keyboard &k)
{
    releaseKey(k.key());
}


void ChaosSynthApp::randomizeNoisy()
{
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        synth[i].resetNoisy();
    }
}

void ChaosSynthApp::randomizeClean()
{
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        synth[i].resetClean();
    }
}

void ChaosSynthApp::trigger(int id)
{
    ChaosSynthParameters params;
    params.id = id;

    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
//        if (synth[i].done()) {
            synth[i].trigger(params);
            break;
//        }
    }
}

void ChaosSynthApp::release(int id)
{
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
//        if (synth[i].done()) {
            synth[i].release();
            break;
//        }
    }
}

void ChaosSynthApp::triggerKey(int id)
{
    float frequency = 440;
    int keyOffset = (int) keyboardOffset.get();
    trigger(id);
}

void ChaosSynthApp::releaseKey(int id)
{
    release(id);
}

int main(int argc, char *argv[] )
{
    int midiChannel = 1;
    if (argc > 1) {
        midiChannel = atoi(argv[1]);
    }

    ChaosSynthApp app(midiChannel);

    app.initializeValues();
    app.initializePresets(); // Must be called before initializeGui
    app.initializeGui();
    app.initWindow();
#ifdef SURROUND
    int outChans = 8;
    app.outputRouting = {4, 3, 7, 6, 2 };
#else
    int outChans = 2;
    app.outputRouting = {0, 1};
#endif
    app.initAudio(44100, 2048, outChans, 0);
    gam::sampleRate(app.audioIO().fps());

    AudioDevice::printAll();
    app.audioIO().print();

    app.start();
    return 0;
}
