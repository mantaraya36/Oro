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

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Envelope.h"

using namespace std;
using namespace al;

//#define SURROUND

#define NUM_VOICES 8
#define SYNTH_POLYPHONY 16

namespace gam{
template <class Tv=gam::real, class Tp=gam::real, class Td=DomainObserver>
class Mode : public Filter2<Tv,Tp,Td>{
public:

    /// \param[in] frq	Center frequency
    /// \param[in] wid	Bandwidth
    Mode(Tp frq = Tp(1000), Tp wid = Tp(100))
        :	Base(frq, wid)
    {
        onDomainChange(1);
    }

    /// Set center frequency
    void freq(Tp v){
        Base::freqRef(v);
        mSin = scl::cosP3<Tp>(scl::foldOnce<Tp>(v - Tp(0.25), Tp(0.5)));
        computeGain();
    }

    /// Set bandwidth
    void width(Tp v){ Base::width(v); computeGain(); }

    void set(Tp frq, Tp wid){ Base::width(wid); freq(frq); }

    /// Filter sample
    Tv operator()(Tv in){
        Tv t = in * gain() + d1*mC[1] + d2*mC[2];
        this->delay(t);
        return t;
    }

    void onDomainChange(double r){ freq(mFreq); width(mWidth); }

protected:
    INHERIT_FILTER2;
    Tp mSin;

    // compute constant gain factor
    void computeGain(){ gain() = 1.0; /*(Tp(1) - mRad*mRad) * mSin;*/ }
};
}

class ModalSynthParameters {
public:
    int id; // Instance id (e.g. MIDI note)
    float mLevel;
    float mFundamental;
    float mFrequencyFactors[NUM_VOICES];
    float mWidths[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];

    // Spatialization
    float mArcStart;
    float mArcSpan;
    int mOutputChannel;
    vector<int> mOutputRouting;
};

class ModalSynth {
public:
    ModalSynth(){
    }

    void trigger(ModalSynthParameters &params) {
        mId = params.id;
        mLevel = params.mLevel;
        memcpy(mFrequencyFactors, params.mFrequencyFactors, sizeof(float) * NUM_VOICES); // Must be called before settinf oscillator fundamental
        memcpy(mWidths, params.mWidths, sizeof(float) * NUM_VOICES); // Must be called before settinf oscillator fundamental
        memcpy(mAmplitudes, params.mAmplitudes, sizeof(float) * NUM_VOICES);
//        setFilterFreq(params.mFundamental);
        mNoiseEnv.reset();

        for (int i = 0; i < NUM_VOICES; i++) {
            mResonators[i].set(params.mFundamental * mFrequencyFactors[i], mWidths[i] * params.mFundamental * mFrequencyFactors[i]);
        }
        mDone = false;
        mOutputChannel = params.mOutputChannel;
    }

    void generateAudio(AudioIOData &io) {
        float noise;
        float max = 0.0;
        while (io()) {
            noise = mNoise() * mNoiseEnv();
            for (int i = 0; i < NUM_VOICES; i++) {
                float value = mResonators[i](noise) * mAmplitudes[i] *  std::pow(10, (mLevel-100)/ 20.0);
				io.out(mOutputChannel) +=  value;
                if(value > max) {max = value;};
			}
        }
        if(max < 0.000001) {mDone = true;}
    }

//    void setFilterFreq(float frequency)
//    {
//        for (int i = 0; i < NUM_VOICES; i++) {
//            mResonators[i].freq(frequency * mFrequencyFactors[i]);
//        }
//    }

    bool done() {
        return mDone;
    }

private:

    // Instance parameters

    // Synthesis
    gam::Mode<> mResonators[NUM_VOICES];
    float mWidths[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];
    gam::AD<> mNoiseEnv {0.001f, 0.001f};
    gam::NoiseBrown<> mNoise;
    int mOutputChannel;

    int mId = 0;
    float mLevel = 0;
    float mFrequencyFactors[NUM_VOICES];
    bool mDone {true};

    int mOutMap[NUM_VOICES] = {0, 1, 0, 1, 0, 1, 0, 1};
};

class ModalSynthApp: public App
{
public:
    ModalSynthApp()
    {
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
//    virtual void onKeyUp(const Keyboard& k) override;

    //
    void multiplyAmps(float factor);
    void randomizeAmps(float max, bool sortPartials = true);

    //
    void multiplyWidths(float factor);
    void randomizeWidths(float max, bool sortPartials = true);

    // Transform partials
    void multiplyFactors(float factor);
    void randomizeFactors(float max, bool sortPartials = true);

    // Envelope
    void trigger(int id);
    void release(int id);

    vector<int> outputRouting;
private:

    // Parameters
    Parameter mLevel {"Level (dB)", "", 70.0, "", 0, 100.0};
    Parameter mFundamental {"Fundamental", "", 55.0, "", 0.0, 9000.0};

    vector<Parameter> mFactors = {
        {"factor1", "", 1.0f, "", 1.0, 20.0},
        {"factor2", "", 2.1f, "", 1.0, 20.0},
        {"factor3", "", 4.4f, "", 1.0, 20.0},
        {"factor4", "", 4.7f, "", 1.0, 20.0},
        {"factor5", "", 5.8f, "", 1.0, 20.0},
        {"factor6", "", 6.1f, "", 1.0, 20.0},
        {"factor7", "", 7.1f, "", 1.0, 20.0},
        {"factor8", "", 7.2f, "", 1.0, 20.0}
    };
    vector<Parameter> mWidths = {
        {"width1", "", 0.001f, "", 0.0000001, 3.0},
        {"width2", "", 0.001f, "", 0.0000001, 3.0},
        {"width3", "", 0.001f, "", 0.0000001, 3.0},
        {"width4", "", 0.001f, "", 0.0000001, 3.0},
        {"width5", "", 0.001f, "", 0.0000001, 3.0},
        {"width6", "", 0.001f, "", 0.0000001, 3.0},
        {"width7", "", 0.001f, "", 0.0000001, 3.0},
        {"width8", "", 0.001f, "", 0.0000001, 3.0}
    };
    vector<Parameter> mAmplitudes = {
        {"amp1", "", 1.0f, "", 0.0, 3.0},
        {"amp2", "", 1.0f, "", 0.0, 3.0},
        {"amp3", "", 1.0f, "", 0.0, 3.0},
        {"amp4", "", 1.0f, "", 0.0, 3.0},
        {"amp5", "", 1.0f, "", 0.0, 3.0},
        {"amp6", "", 1.0f, "", 0.0, 3.0},
        {"amp7", "", 1.0f, "", 0.0, 3.0},
        {"amp8", "", 1.0f, "", 0.0, 3.0}
    };
    // Presets
    PresetHandler mPresetHandler {"modalPresets"};

    //Sequencing
    PresetSequencer sequencer;
    SequenceRecorder recorder;

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

    ParameterBool midiLight{"MIDI", "", false};

    MIDIIn midiIn {"USB Oxygen 49"};

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    static void midiCallback(double deltaTime, std::vector<unsigned char> *msg, void *userData){
        ModalSynthApp *app = static_cast<ModalSynthApp *>(userData);
        unsigned numBytes = msg->size();
        app->midiLight.set(true);
//        midiLightWaiter(app);

        if(numBytes > 0){
            unsigned char status = msg->at(0);
            if(MIDIByte::isChannelMessage(status)){
                unsigned char type = status & MIDIByte::MESSAGE_MASK;
                unsigned char chan = status & MIDIByte::CHANNEL_MASK;
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
                    app->mPresetHandler.recallPreset(msg->at(1));
                break;
                default:;
                }
            }
        }
    }

    // Synthesis
    ModalSynth synth[SYNTH_POLYPHONY];
};

void ModalSynthApp::initializeValues()
{
//    mLevel.setProcessingCallback([](float value, void *userData) {
//        value = std::pow(10, (value-100)/ 20.0); // db to lin ampl
//        return value;
//    }, nullptr);
    mFundamental.set(880);
    mLevel.set(70);
}

void ModalSynthApp::initializeGui()
{
    gui << SequencerGUI::makePresetHandlerView(mPresetHandler, 1.0, 12, 4);
    gui << mLevel << mFundamental;
// //   gui << mCumulativeDelay << mCumulativeDelayRandomness;
//    gui << mArcStart << mArcSpan;
//	gui << mAttackCurve << mReleaseCurve;
    gui << midiLight;

//    gui << SequencerGUI::makeSequencerPlayerView(sequencer)
//        << SequencerGUI::makeRecorderView(recorder);
//    // Right side

//    box << ParameterGUI::makeParameterView(mLevel);


    glv::Button *ampModRampUpButton = new glv::Button;
    ampModRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyAmps(1.01);
        }
    }, glv::Update::Value, this);
    ampModRampUpButton->property(glv::Momentary, true);
    ampsBox << ampModRampUpButton << new glv::Label("Ramp up");

    glv::Button *ampsRampDownButton = new glv::Button;
    ampsRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyAmps(1/1.01);
        }
    }, glv::Update::Value, this);
    ampsRampDownButton->property(glv::Momentary, true);
    ampsBox << ampsRampDownButton << new glv::Label("Ramp down");

	glv::Button *randomizeAmpsButton = new glv::Button;
    randomizeAmpsButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->randomizeAmps(8.0);
        }
    }, glv::Update::Value, this);
    randomizeAmpsButton->property(glv::Momentary, true);
    ampsBox << randomizeAmpsButton << new glv::Label("Randomize Sort");

    for (int i = 0; i < NUM_VOICES; i++) {
        ampsBox << ParameterGUI::makeParameterView(mAmplitudes[i]);
    }
    ampsBox.fit();
    ampsBox.enable(glv::DrawBack);
    ampsBox.anchor(glv::Place::TR);
    ampsBox.set(-540, 30, ampsBox.width(), ampsBox.height());
    controlView << ampsBox;

//    // View selector

    glv::Button *widthsModRampUpButton = new glv::Button;
    widthsModRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyWidths(1.03);
        }
    }, glv::Update::Value, this);
    widthsModRampUpButton->property(glv::Momentary, true);
    widthsBox << widthsModRampUpButton << new glv::Label("Ramp up");

    glv::Button *widthsRampDownButton = new glv::Button;
    widthsRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyWidths(1/1.03);
        }
    }, glv::Update::Value, this);
    widthsRampDownButton->property(glv::Momentary, true);
    widthsBox << widthsRampDownButton << new glv::Label("Ramp down");

	glv::Button *randomizeWidthsButton = new glv::Button;
    randomizeWidthsButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->randomizeWidths(0.1);
        }
    }, glv::Update::Value, this);
    randomizeWidthsButton->property(glv::Momentary, true);
    widthsBox << randomizeWidthsButton << new glv::Label("Randomize Sort");

    for (int i = 0; i < NUM_VOICES; i++) {
        widthsBox << ParameterGUI::makeParameterView(mWidths[i]);
    }
    widthsBox.fit();
    widthsBox.enable(glv::DrawBack);
    widthsBox.anchor(glv::Place::TR);
    widthsBox.set(-180, 30, widthsBox.width(), widthsBox.height());
    controlView << widthsBox;

//    // Amp Mod

//    glv::Button *ampModUpButton = new glv::Button;
//    ampModUpButton->attach([](const glv::Notification &n) {
//        glv::Button *b = n.sender<glv::Button>();
//        ModalSynthApp *app = n.receiver<ModalSynthApp>();
//        if (b->getValue() == 1) {
//            app->ampModFreqAdd(0.05);
//        }
//    }, glv::Update::Value, this);
//    ampModUpButton->property(glv::Momentary, true);
//    controlsBox << ampModUpButton << new glv::Label("Higher freq");

//    glv::Button *ampModDownButton = new glv::Button;
//    ampModDownButton->attach([](const glv::Notification &n) {
//        glv::Button *b = n.sender<glv::Button>();
//        ModalSynthApp *app = n.receiver<ModalSynthApp>();
//        if (b->getValue() == 1) {
//            app->ampModFreqAdd(-0.05);
//        }
//    }, glv::Update::Value, this);
//    ampModDownButton->property(glv::Momentary, true);
//    controlsBox << ampModDownButton << new glv::Label("Lower freq");

    glv::Button *factorsModRampUpButton = new glv::Button;
    factorsModRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyFactors(1.01);
        }
    }, glv::Update::Value, this);
    factorsModRampUpButton->property(glv::Momentary, true);
    controlsBox << factorsModRampUpButton << new glv::Label("Ramp up");

    glv::Button *factorsRampDownButton = new glv::Button;
    factorsRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyFactors(1/1.01);
        }
    }, glv::Update::Value, this);
    factorsRampDownButton->property(glv::Momentary, true);
    controlsBox << factorsRampDownButton << new glv::Label("Ramp down");

	glv::Button *randomizeFactorsButton = new glv::Button;
    randomizeFactorsButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        ModalSynthApp *app = n.receiver<ModalSynthApp>();
        if (b->getValue() == 1) {
            app->randomizeFactors(8.0);
        }
    }, glv::Update::Value, this);
    randomizeFactorsButton->property(glv::Momentary, true);
    controlsBox << randomizeFactorsButton << new glv::Label("Randomize Sort");

    for (int i = 0; i < NUM_VOICES; i++) {
        controlsBox << ParameterGUI::makeParameterView(mFactors[i]);
    }
    controlsBox.fit();
    controlsBox.enable(glv::DrawBack);
    controlsBox.anchor(glv::Place::TR);
    controlsBox.set(-360, 30, controlsBox.width(), controlsBox.height());
    controlView << controlsBox;

    controlView.set(300, 0, 500, 400);
}

void ModalSynthApp::initializePresets()
{
    mPresetHandler << mLevel << mFundamental;
//    mPresetHandler << mCumulativeDelay << mCumulativeDelayRandomness;
//    mPresetHandler << mArcStart << mArcSpan;
    for (int i = 0; i < NUM_VOICES; i++) {
        mPresetHandler << mFactors[i] << mWidths[i] << mAmplitudes[i];
    }
//    mPresetHandler.print();
    sequencer << mPresetHandler;
    recorder << mPresetHandler;

    // MIDI Control of parameters
    unsigned int midiControllerPort = 0;
    parameterMIDI.init(midiControllerPort);
//    parameterMIDI.connectControl(mCumulativeDelay, 75, 1);
//    parameterMIDI.connectControl(mCumulativeDelayRandomness, 76, 1);

    // MIDI control of presets
    // 74 71 91 93 73 72 5 84 7
    // 75 76 77 78 74 71 24 102
    presetMIDI.init(midiControllerPort, mPresetHandler);
    presetMIDI.setMorphControl(102, 1, 0.0, 8.0);
    // MIDI preset mapping
//    presetMIDI.connectNoteToPreset(1, 0, 36, 24, 59);
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

    midiIn.setCallback(ModalSynthApp::midiCallback, this);
}

void ModalSynthApp::onSound(AudioIOData &io)
{
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        if (!synth[i].done()) {
            synth[i].generateAudio(io);
            io.frame(0);
        }
    }
}

void ModalSynthApp::onKeyDown(const Keyboard &k)
{
    trigger(k.key());
}

void ModalSynthApp::multiplyAmps(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmplitudes[i].set(mAmplitudes[i].get() * factor);
    }
}

void ModalSynthApp::randomizeAmps(float max, bool sortPartials)
{
    if (max > 1.0) {
        max = 1.0;
    } else if (max < 0.01) {
        max = 0.01;
    }
    srand(time(0));
    vector<float> randomFactors(NUM_VOICES);
    for (int i = 0; i < NUM_VOICES; i++) {
        int random_variable = std::rand();
        randomFactors[i] = 0.000001 + (max *random_variable/(float) RAND_MAX);
    }
    if (sortPartials) {
        sort(randomFactors.begin(), randomFactors.end());
    }
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmplitudes[i].set(randomFactors[i]);
    }
}

void ModalSynthApp::multiplyWidths(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mWidths[i].set(mWidths[i].get() * factor);
    }
}

void ModalSynthApp::randomizeWidths(float max, bool sortPartials)
{
    if (max > 0.1) {
        max = 0.1;
    } else if (max < 0.001) {
        max = 0.001;
    }
    srand(time(0));
    vector<float> randomFactors(NUM_VOICES);
    for (int i = 0; i < NUM_VOICES; i++) {
        int random_variable = std::rand();
        randomFactors[i] = 0.001 + (max *random_variable/(float) RAND_MAX);
    }
    if (sortPartials) {
        sort(randomFactors.begin(), randomFactors.end());
    }
    for (int i = 0; i < NUM_VOICES; i++) {
        mWidths[i].set(randomFactors[i]);
    }
}

void ModalSynthApp::multiplyFactors(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mFactors[i].set(mFactors[i].get() * factor);
    }
}

void ModalSynthApp::randomizeFactors(float max, bool sortPartials)
{
    if (max > 25.0) {
        max = 25.0;
    } else if (max < 1) {
        max = 1.0;
    }
    srand(time(0));
    vector<float> randomFactors(NUM_VOICES);
    for (int i = 0; i < NUM_VOICES; i++) {
        int random_variable = std::rand();
        randomFactors[i] = 1 + (max *random_variable/(float) RAND_MAX);
    }
    if (sortPartials) {
        sort(randomFactors.begin(), randomFactors.end());
    }
    for (int i = 0; i < NUM_VOICES; i++) {
        mFactors[i].set(randomFactors[i]);
    }
}


void ModalSynthApp::trigger(int id)
{
    ModalSynthParameters params;
    params.id = id;
    params.mLevel = mLevel.get();
    params.mFundamental = mFundamental.get();
//    params.mCumulativeDelay = mCumulativeDelay.get();
//    params.mCumDelayRandomness = mCumulativeDelayRandomness.get();
//    params.mArcStart = mArcStart.get();
//    params.mArcSpan = mArcSpan.get();
    params.mOutputRouting = outputRouting;
    params.mOutputChannel = 2;


    for (int i = 0; i < NUM_VOICES; i++) {
        params.mFrequencyFactors[i] = mFactors[i].get();
        params.mAmplitudes[i] = mAmplitudes[i].get();
        params.mWidths[i] = mWidths[i].get();

    }
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        if (synth[i].done()) {
            synth[i].trigger(params);
            break;
        }
    }
}

int main(int argc, char *argv[] )
{
    ModalSynthApp app;

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
