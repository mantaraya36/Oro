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

#define SURROUND

#define NUM_VOICES 16

class AddSynthParameters {
public:
    float mLevel;
    float mFundamental;
    float mCumulativeDelay;
    float mAttackTimes[NUM_VOICES];
    float mReleaseTimes[NUM_VOICES];
    float mFrequencyFactors[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];

    // Spatialization
    float mArcStart;
    float mArcSpan;
    vector<int> mOutputRouting;
};

class AddSynth {
public:
    AddSynth(){
        float envLevels[5] = {0.0, 0.0, 1.0, 1.0, 0.0};
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].levels(envLevels, 5).sustainPoint(2);
        }
        setCurvature(-4);
        release();
    }

    void trigger(AddSynthParameters &params) {
        mLevel = params.mLevel;
        updateOutMap(params.mArcStart, params.mArcSpan, params.mOutputRouting);
        memcpy(mFrequencyFactors, params.mFrequencyFactors, sizeof(float) * NUM_VOICES); // Must be called before settinf oscillator fundamental
        setOscillatorFundamental(params.mFundamental);
        setInitialCumulativeDelay(params.mCumulativeDelay);
        memcpy(mAmplitudes, params.mAmplitudes, sizeof(float) * NUM_VOICES);

        for (int i = 0; i < NUM_VOICES; i++) {
            setAttackTime(params.mAttackTimes[i], i);
            setReleaseTime(params.mReleaseTimes[i], i);
            mEnvelopes[i].reset();
        }
    }

    void release() {

        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].release();
        }
    }

    void generateAudio(AudioIOData &io) {
        while (io()) {
            for (int i = 0; i < NUM_VOICES; i++) {
                io.out(mOutMap[i]) += mOscillators[i]() * mEnvelopes[i]() *  mAmplitudes[i] * mLevel;
            }
        }
    }

    void setInitialCumulativeDelay(float initialDelay)
    {
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].lengths()[0] = initialDelay * i;
        }
    }

    void setAttackTime(float attackTime, int i)
    {
        mEnvelopes[i].lengths()[1] = attackTime;
    }

    void setReleaseTime(float releaseTime, int i)
    {
        mEnvelopes[i].lengths()[3] = releaseTime;
    }

    void setCurvature(float curvature)
    {
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].curve(curvature);
        }
    }

    void setOscillatorFundamental(float frequency)
    {
        for (int i = 0; i < NUM_VOICES; i++) {
            mOscillators[i].freq(frequency * mFrequencyFactors[i]);
        }
    }

    void updateOutMap(float arcStart, float arcSpan, vector<int> outputRouting) {
        int numSpeakers = outputRouting.size();
        for (int i = 0; i < NUM_VOICES; i++) {
            mOutMap[i] = outputRouting[fmod(((arcStart + (arcSpan * i/(float) (NUM_VOICES))) * numSpeakers ), numSpeakers)];
            std::cout << mOutMap[i] << std::endl;
        }
    }

private:

    // Instance parameters

    // Synthesis
    gam::Sine<> mOscillators[NUM_VOICES];
    gam::Env<4> mEnvelopes[NUM_VOICES]; // First segment determines envelope delay

    float mLevel = 0;
    float mFrequencyFactors[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];

    int mOutMap[NUM_VOICES] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

};


class AddSynthApp: public App
{
public:
    AddSynthApp()
    {
        initializeValues();
        initWindow();
#ifdef SURROUND
        int outChans = 8;
        outputRouting = {4, 3, 7, 6, 2 };
#else
        int outChans = 2;
        outputRouting = {0, 1};
#endif
        initAudio(44100, 256, outChans, 0);
        gam::sampleRate(audioIO().fps());

        initializePresets(); // Must be called before initializeGui
        initializeGui();
    }

    void initializeValues();
    void initializeGui();
    void initializePresets();

    virtual void onCreate(const ViewpointWindow& win) override {
        gui.setParentWindow(*windows()[0]);

        controlView.parentWindow(*windows()[0]);
    }

    virtual void onSound(AudioIOData &io) override;

    virtual void onKeyDown(const Keyboard& k) override;
    virtual void onKeyUp(const Keyboard& k) override;

    // Transform partials
    void multiplyPartials(float factor);
    void randomizePartials(float max);
    void harmonicPartials();
    void oddPartials();

    // Transform amplitudes
    void setAmpsToOne();
    void setAmpsOneOverN();
    void ampSlopeFactor(double factor);

    // Envelope
    void trigger();
    void release();
    void setInitialCumulativeDelay(float initialDelay);
    void setAttackTime(float attackTime);
    void setReleaseTime(float releaseTime);
    void setCurvature(float curvature);

private:

    // Parameters
    Parameter mLevel {"Level", "", 0.25, "", 0.0, 1.0};
    Parameter mFundamental {"Fundamental", "", 220.0, "", 0.0, 9000.0};
    Parameter mCumulativeDelay{"CumDelay", "", 0.0, "", 0.0, 0.2};

    // Spatialization
    int mNumSpeakers; // Assumes regular angular separation
    Parameter mArcStart {"ArcStart", "", 0, "", 0.0, 1.0}; // 0 -> 1 (fraction of circle)
    Parameter mArcSpan {"ArcSpan", "", 0, "", 0.0, 2.0}; // 1 = full circumference. + -> CW, - -> CCW

    vector<Parameter> mAttackTimes = {
        {"Attack1", "", 0.1f, "", 0.0, 20.0},
        {"Attack2", "", 0.1f, "", 0.0, 20.0},
        {"Attack3", "", 0.1f, "", 0.0, 20.0},
        {"Attack4", "", 0.1f, "", 0.0, 20.0},
        {"Attack5", "", 0.1f, "", 0.0, 20.0},
        {"Attack6", "", 0.1f, "", 0.0, 20.0},
        {"Attack7", "", 0.1f, "", 0.0, 20.0},
        {"Attack8", "", 0.1f, "", 0.0, 20.0},
        {"Attack9", "", 0.1f, "", 0.0, 20.0},
        {"Attack10", "", 0.1f, "", 0.0, 20.0},
        {"Attack11", "", 0.1f, "", 0.0, 20.0},
        {"Attack12", "", 0.1f, "", 0.0, 20.0},
        {"Attack13", "", 0.1f, "", 0.0, 20.0},
        {"Attack14", "", 0.1f, "", 0.0, 20.0},
        {"Attack15", "", 0.1f, "", 0.0, 20.0},
        {"Attack16", "", 0.1f, "", 0.0, 20.0}
                                              };
    vector<Parameter> mReleaseTimes = {
        {"Release1", "", 1.0f, "", 0.0, 20.0},
        {"Release2", "", 1.0f, "", 0.0, 20.0},
        {"Release3", "", 1.0f, "", 0.0, 20.0},
        {"Release4", "", 1.0f, "", 0.0, 20.0},
        {"Release5", "", 1.0f, "", 0.0, 20.0},
        {"Release6", "", 1.0f, "", 0.0, 20.0},
        {"Release7", "", 1.0f, "", 0.0, 20.0},
        {"Release8", "", 1.0f, "", 0.0, 20.0},
        {"Release9", "", 1.0f, "", 0.0, 20.0},
        {"Release10", "", 1.0f, "", 0.0, 20.0},
        {"Release11", "", 1.0f, "", 0.0, 20.0},
        {"Release12", "", 1.0f, "", 0.0, 20.0},
        {"Release13", "", 1.0f, "", 0.0, 20.0},
        {"Release14", "", 1.0f, "", 0.0, 20.0},
        {"Release15", "", 1.0f, "", 0.0, 20.0},
        {"Release16", "", 1.0f, "", 0.0, 20.0}
                                              };

    vector<Parameter> mFrequencyFactors = {
        {"Harm1", "", 1.0f, "", 0.0, 20.0},
        {"Harm2", "", 2.0f, "", 0.0, 20.0},
        {"Harm3", "", 3.0f, "", 0.0, 20.0},
        {"Harm4", "", 4.0f, "", 0.0, 20.0},
        {"Harm5", "", 5.0f, "", 0.0, 20.0},
        {"Harm6", "", 6.0f, "", 0.0, 20.0},
        {"Harm7", "", 7.0f, "", 0.0, 20.0},
        {"Harm8", "", 8.0f, "", 0.0, 20.0},
        {"Harm9", "", 9.0f, "", 0.0, 20.0},
        {"Harm10", "", 10.0f, "", 0.0, 20.0},
        {"Harm11", "", 11.0f, "", 0.0, 20.0},
        {"Harm12", "", 12.0f, "", 0.0, 20.0},
        {"Harm13", "", 13.0f, "", 0.0, 20.0},
        {"Harm14", "", 14.0f, "", 0.0, 20.0},
        {"Harm15", "", 15.0f, "", 0.0, 20.0},
        {"Harm16", "", 16.0f, "", 0.0, 20.0}
                                              };
    vector<Parameter> mAmplitudes = {
        {"Amp1", "", 1.0f, "", 0.0, 1.0},
        {"Amp2", "", 1/2.0f, "", 0.0, 1.0},
        {"Amp3", "", 1/3.0f, "", 0.0, 1.0},
        {"Amp4", "", 1/4.0f, "", 0.0, 1.0},
        {"Amp5", "", 1/5.0f, "", 0.0, 1.0},
        {"Amp6", "", 1/6.0f, "", 0.0, 1.0},
        {"Amp7", "", 1/7.0f, "", 0.0, 1.0},
        {"Amp8", "", 1/8.0f, "", 0.0, 1.0},
        {"Amp9", "", 1/9.0f, "", 0.0, 1.0},
        {"Amp10", "", 1/10.0f, "", 0.0, 1.0},
        {"Amp11", "", 1/11.0f, "", 0.0, 1.0},
        {"Amp12", "", 1/12.0f, "", 0.0, 1.0},
        {"Amp13", "", 1/13.0f, "", 0.0, 1.0},
        {"Amp14", "", 1/14.0f, "", 0.0, 1.0},
        {"Amp15", "", 1/15.0f, "", 0.0, 1.0},
        {"Amp16", "", 1/16.0f, "", 0.0, 1.0}
                                              };
    // Presets
    PresetHandler mPresetHandler;

    //Sequencing
    PresetSequencer sequencer;
    SequenceRecorder recorder;

    // GUI
    ParameterGUI gui;

    glv::DropDown viewSelector;
    glv::Box harmonicsBox {glv::Direction::S};
    glv::Box amplitudesBox {glv::Direction::S};
    glv::Box attackTimesBox {glv::Direction::S};
    glv::Box releaseTimesBox {glv::Direction::S};
    GLVDetachable controlView;

    // MIDI Control
    PresetMIDI presetMIDI;
    ParameterMIDI parameterMIDI;

    // Synthesis
    AddSynth synth;
    vector<int> outputRouting;
};

void AddSynthApp::initializeValues()
{
    mFundamental.set(220);
    harmonicPartials();

    mLevel.set(0.25);
    mCumulativeDelay.set(0.0);
    for (int i = 0; i < NUM_VOICES; i++) {
        mAttackTimes[i].set(0.1);
        mReleaseTimes[i].set(2.0);
    }
}

void AddSynthApp::initializeGui()
{
    gui << SequencerGUI::makePresetHandlerView(mPresetHandler, 1.0, 12, 4);
    gui << mLevel << mFundamental << mCumulativeDelay;
    gui << mArcStart << mArcSpan;

    gui << SequencerGUI::makeSequencerPlayerView(sequencer)
        << SequencerGUI::makeRecorderView(recorder);
    // Right side

//    box << ParameterGUI::makeParameterView(mLevel);

    // View selector

    // Harmonics boxes
    glv::Button *compressButton = new glv::Button;
    compressButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->multiplyPartials(0.99);
    }, glv::Update::Value, this);
    compressButton->property(glv::Momentary, true);
    harmonicsBox << compressButton << new glv::Label("Compress");

    glv::Button *expandButton = new glv::Button;
    expandButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->multiplyPartials(1/0.99);
    }, glv::Update::Value, this);
    expandButton->property(glv::Momentary, true);
    harmonicsBox << expandButton << new glv::Label("Expand");

    glv::Button *randomizeButton = new glv::Button;
    randomizeButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->randomizePartials(10.0);
    }, glv::Update::Value, this);
    randomizeButton->property(glv::Momentary, true);
    harmonicsBox << randomizeButton << new glv::Label("Randomize");

    glv::Button *harmonicButton = new glv::Button;
    harmonicButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->harmonicPartials();
    }, glv::Update::Value, this);
    harmonicButton->property(glv::Momentary, true);
    harmonicsBox << harmonicButton << new glv::Label("Harmonic");

    glv::Button *evenButton = new glv::Button;
    evenButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->oddPartials();
    }, glv::Update::Value, this);
    evenButton->property(glv::Momentary, true);
    harmonicsBox << evenButton << new glv::Label("Even only");

    for (int i = 0; i < NUM_VOICES; i++) {
        harmonicsBox << ParameterGUI::makeParameterView(mFrequencyFactors[i]);
    }
    harmonicsBox.fit();
    harmonicsBox.enable(glv::DrawBack);
    harmonicsBox.anchor(glv::Place::TR);
    harmonicsBox.set(-360, 30, harmonicsBox.width(), harmonicsBox.height());
    controlView << harmonicsBox;

    // Amplitudes

    glv::Button *oneButton = new glv::Button;
    oneButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->setAmpsToOne();
    }, glv::Update::Value, this);
    oneButton->property(glv::Momentary, true);
    amplitudesBox << oneButton << new glv::Label("All One");

    glv::Button *oneOverNButton = new glv::Button;
    oneOverNButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->setAmpsOneOverN();
    }, glv::Update::Value, this);
    oneOverNButton->property(glv::Momentary, true);
    amplitudesBox << oneOverNButton << new glv::Label("One over N");

    glv::Button *slopeUpButton = new glv::Button;
    slopeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->ampSlopeFactor(1.005);
    }, glv::Update::Value, this);
    slopeUpButton->property(glv::Momentary, true);
    amplitudesBox << slopeUpButton << new glv::Label("Slope up");

    glv::Button *slopeDownButton = new glv::Button;
    slopeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->ampSlopeFactor(1/1.005);
    }, glv::Update::Value, this);
    slopeDownButton->property(glv::Momentary, true);
    amplitudesBox << slopeDownButton << new glv::Label("Slope down");

    for (int i = 0; i < NUM_VOICES; i++) {
        amplitudesBox << ParameterGUI::makeParameterView(mAmplitudes[i]);
    }
    amplitudesBox.fit();
    amplitudesBox.enable(glv::DrawBack);
    amplitudesBox.anchor(glv::Place::TR);
    amplitudesBox.set(-360, 30, amplitudesBox.width(), amplitudesBox.height());
    amplitudesBox.disable(glv::Property::Visible);
    controlView << amplitudesBox;

    for (int i = 0; i < NUM_VOICES; i++) {
        attackTimesBox << ParameterGUI::makeParameterView(mAttackTimes[i]);
    }
    attackTimesBox.fit();
    attackTimesBox.enable(glv::DrawBack);
    attackTimesBox.anchor(glv::Place::TR);
    attackTimesBox.set(-360, 30, attackTimesBox.width(), attackTimesBox.height());
    attackTimesBox.disable(glv::Property::Visible);
    controlView << attackTimesBox;

    for (int i = 0; i < NUM_VOICES; i++) {
        releaseTimesBox << ParameterGUI::makeParameterView(mReleaseTimes[i]);
    }
    releaseTimesBox.fit();
    releaseTimesBox.enable(glv::DrawBack);
    releaseTimesBox.anchor(glv::Place::TR);
    releaseTimesBox.set(-360, 30, releaseTimesBox.width(), releaseTimesBox.height());
    releaseTimesBox.disable(glv::Property::Visible);
    controlView << releaseTimesBox;


    viewSelector.addItem("Harmonics").addItem("Amplitudes").addItem("Attack").addItem("Release");
    viewSelector.attach([](const glv::Notification &n) {
        glv::DropDown *b = n.sender<glv::DropDown>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        std::cout << "pressed " << b->selectedItem() << std::endl;
        app->harmonicsBox.disable(glv::Property::Visible);
        app->amplitudesBox.disable(glv::Property::Visible);
        app->attackTimesBox.disable(glv::Property::Visible);
        app->releaseTimesBox.disable(glv::Property::Visible);
        switch (b->selectedItem()) {
        case 0:
            app->harmonicsBox.enable(glv::Property::Visible);
            break;
        case 1:
            app->amplitudesBox.enable(glv::Property::Visible);
            break;
        case 2:
            app->attackTimesBox.enable(glv::Property::Visible);
            break;
        case 3:
            app->releaseTimesBox.enable(glv::Property::Visible);
            break;
        }
    }, glv::Update::Value, this);
    viewSelector.enable(glv::DrawBack);
    viewSelector.anchor(glv::Place::TR);
    viewSelector.set(-360, 0, viewSelector.width(), viewSelector.height());
    controlView << viewSelector;

    controlView.set(300, 0, 300, 400);
}

void AddSynthApp::initializePresets()
{
    mPresetHandler << mLevel;
    mPresetHandler << mFundamental << mCumulativeDelay;
    mPresetHandler << mArcStart << mArcSpan;
    for (int i = 0; i < NUM_VOICES; i++) {
        mPresetHandler << mFrequencyFactors[i] << mAmplitudes[i];
        mPresetHandler << mAttackTimes[i] << mReleaseTimes[i];
    }
    mPresetHandler.print();

    // MIDI Control of parameters
    unsigned int midiControllerPort = 0;
    parameterMIDI.init(midiControllerPort);
//    parameterMIDI.connectControl(frequency, 1, 1);

    // MIDI control of presets
    presetMIDI.init(midiControllerPort, mPresetHandler);
    presetMIDI.setMorphControl(2, 1, 0.0, 8.0);
    // MIDI preset mapping
    presetMIDI.connectNoteToPreset(1, 0, 48, 24, 71);

    sequencer << mPresetHandler;
    recorder << mPresetHandler;
}

void AddSynthApp::onSound(AudioIOData &io)
{
    synth.generateAudio(io);
}

void AddSynthApp::onKeyDown(const Keyboard &k)
{
    std::cout << "down" << std::endl;
    trigger();
}

void AddSynthApp::onKeyUp(const Keyboard &k)
{
    release();
}

void AddSynthApp::multiplyPartials(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        float expFactor = pow(factor, i);
        mFrequencyFactors[i].set(mFrequencyFactors[i].get() * expFactor);
    }
}

void AddSynthApp::randomizePartials(float max)
{
    if (max > 20.0) {
        max = 20.0;
    } else if (max < 1) {
        max = 1.0;
    }
    srand(time(0));
    vector<float> randomFactors(NUM_VOICES);
    for (int i = 0; i < NUM_VOICES; i++) {
        int random_variable = std::rand();
        randomFactors[i] = 1 + (max *random_variable/(float) RAND_MAX);
    }
    sort(randomFactors.begin(), randomFactors.end());
    for (int i = 0; i < NUM_VOICES; i++) {
        mFrequencyFactors[i].set(randomFactors[i]);
    }
}

void AddSynthApp::harmonicPartials()
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mFrequencyFactors[i].set(i + 1);
    }
}

void AddSynthApp::oddPartials()
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mFrequencyFactors[i].set((i * 2) + 1);
    }
}

void AddSynthApp::setAmpsToOne()
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmplitudes[i].set(1.0);
    }
}

void AddSynthApp::setAmpsOneOverN()
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmplitudes[i].set(1.0/ (i + 1));
    }
}

void AddSynthApp::ampSlopeFactor(double factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmplitudes[i].set(mAmplitudes[i].get() * pow(factor, i));
    }
}

void AddSynthApp::trigger()
{
    AddSynthParameters params;
    params.mLevel = mLevel.get();
    params.mFundamental = mFundamental.get();
    params.mCumulativeDelay = mCumulativeDelay.get();
    params.mArcStart = mArcStart.get();
    params.mArcSpan = mArcSpan.get();
    params.mOutputRouting = outputRouting;
    for (int i = 0; i < NUM_VOICES; i++) {
        params.mAttackTimes[i] = mAttackTimes[i].get();
        params.mReleaseTimes[i] = mReleaseTimes[i].get();
        params.mFrequencyFactors[i] = mFrequencyFactors[i].get();
        params.mAmplitudes[i] = mAmplitudes[i].get();
    }
    synth.trigger(params);
}

void AddSynthApp::release()
{
    synth.release();
}

int main(int argc, char *argv[] )
{
    AddSynthApp().start();

    return 0;
}