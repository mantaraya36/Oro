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

#define SYNTH_POLYPHONY 1

static inline float midi2cps(int midiNote) {
    return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
}

class ChaosSynthParameters {
public:
    int id; // Instance id (e.g. MIDI note)
    float mLevel;
//    float mFundamental;
//    float mFrequencyFactors[NUM_VOICES];
//    float mWidths[NUM_VOICES];
//    float mAmplitudes[NUM_VOICES];

//    // Spatialization
//    float mArcStart;
//    float mArcSpan;
//    int mOutputChannel;
//    vector<int> mOutputRouting;
};

class ChaosSynth {
public:
    ChaosSynth(){
//        for (int i = 0; i < NUM_VOICES; i++) {
//            mResonators[i].type(gam::BAND_PASS_UNIT);
//        }
        connectCallbacks();

        mEnv.sustainPoint(1);


        resetClean();
    }

    void trigger(ChaosSynthParameters &params) {
//        mId = params.id;
        mLevel = params.mLevel;
//        memcpy(mFrequencyFactors, params.mFrequencyFactors, sizeof(float) * NUM_VOICES); // Must be called before settinf oscillator fundamental
//        memcpy(mWidths, params.mWidths, sizeof(float) * NUM_VOICES); // Must be called before settinf oscillator fundamental
//        memcpy(mAmplitudes, params.mAmplitudes, sizeof(float) * NUM_VOICES);
////        setFilterFreq(params.mFundamental);
//        mNoiseEnv.reset();

//        for (int i = 0; i < NUM_VOICES; i++) {
//            mResonators[i].freq(params.mFundamental * mFrequencyFactors[i]);
//            mResonators[i].res(params.mFundamental * mFrequencyFactors[i]/(mWidths[i] * params.mFundamental * mFrequencyFactors[i] / WIDTHFACTOR));
////            mResonators[i].set(params.mFundamental * mFrequencyFactors[i], );
//        }
//        mDone = false;
//        mOutputChannel = params.mOutputChannel;
        mEnv.reset();
//        reset();
    }

    void release() {
        mEnv.release();
    }

    // basstone |
    Parameter phsrFreq1 {"phsrFreq1", "", 440, "", 0.0, 9999.0};
    Parameter phsrFreq2 {"phsrFreq2", "", 440, "", 0.0, 9999.0};
    Parameter bassFilterFreq {"bassFilterFreq", "", 0, "", 0.0, 9999.0};

    gam::Saw<> mOsc1, mOsc2;
    gam::Biquad<> mOscBandPass { 1000, 5, gam::BAND_PASS};
    gam::Biquad<> mOscLowPass {100, 5, gam::LOW_PASS};

    // envelope
    Parameter envFreq1 {"envFreq1", "", 0.2, "", -9.0, 9.0};
    Parameter envFreq2 {"envFreq2", "", -0.21, "", -9.0, 9.0};
    gam::SineR<> mEnvOsc1, mEnvOsc2;

    // noise
    Parameter noiseRnd {"noiseRnd", "", 0.2, "", 0, 22050};
    gam::NoiseWhite<> mNoise;
    float noiseHold {0};
    gam::Accum<> mTrigger;
    gam::AD<> mNoiseEnv{0.003, 0.025};

    // Randomness
    Parameter changeProb {"changeProb", "", 0.02, "", 0, 1};
    Parameter changeDev {"changeDev", "", 0.1, "", 0, 99.0};
    float freqDev1 = 0 , freqDev2 = 0;

    //
    float mLevel;
    al::Reverb<> mReverb;

    gam::BlockDC<> mDCBlockL, mDCBlockR;
    gam::AD<> mEnv{0.5, 3};

    void connectCallbacks() {
        phsrFreq1.registerChangeCallback([] (float value, void *sender,
                                         void *userData, void * blockSender){
            static_cast<ChaosSynth *>(userData)->mOsc1.freq(value);
            std::cout << "new freq " << value << std::endl;
        }, this);
        phsrFreq2.registerChangeCallback([] (float value, void *sender,
                                         void *userData, void * blockSender){
            static_cast<ChaosSynth *>(userData)->mOsc2.freq(value);
        }, this);

        envFreq1.registerChangeCallback([] (float value, void *sender,
                                         void *userData, void * blockSender){
            static_cast<ChaosSynth *>(userData)->mEnvOsc1.freq(value);
        }, this);
        envFreq2.registerChangeCallback([] (float value, void *sender,
                                         void *userData, void * blockSender){
            static_cast<ChaosSynth *>(userData)->mEnvOsc2.freq(value);
        }, this);

        noiseRnd.registerChangeCallback([] (float value, void *sender,
                                         void *userData, void * blockSender){
            static_cast<ChaosSynth *>(userData)->mTrigger.freq(value);
        }, this);

    }

    void resetNoisy()  {
        // envelope |
        float envrnd1 = (rnd::uniform(0, 800) + 200)/5000.0;
        float envrnd2 = ((rnd::uniform(0, 900) + 100.0)/1000.0) - 0.5;
        envFreq1 = envrnd1;
        envFreq2 = envrnd2 * (envrnd1/100.0);

        // basstone |
        float rnd1 = rnd::uniform(0, 12) + 24;
        float rnd2 = (rnd::uniform(0, 100) + 5.0)/400.0;
        phsrFreq1 = midi2cps(rnd1 + rnd2);
        phsrFreq2 = midi2cps(rnd1 - rnd2);
        bassFilterFreq = midi2cps(rnd1 + 21);

        // noise |
        noiseRnd = rnd::uniform(0, 22050);

        changeProb.set(rnd::uniform(0.4));
        changeDev.set(rnd::uniform(20.0));
    }

    void resetClean()  {
        // envelope |
        float envrnd1 = (rnd::uniform(0, 400) + 100)/5000.0;
        float envrnd2 = ((rnd::uniform(0, 90) + 10.0)/1000.0) - 0.05;
        envFreq1 = envrnd1;
        envFreq2 = envrnd2 * (envrnd1/100.0);

        // basstone |
        float rnd1 = rnd::uniform(0, 12) + 44;
        float rnd2 = (rnd::uniform(0, 100) + 5.0)/400.0;
        phsrFreq1 = midi2cps(rnd1 + rnd2);
        phsrFreq2 = midi2cps(rnd1 - rnd2);
        bassFilterFreq = midi2cps(rnd1 + 21);

        // noise |
        noiseRnd = rnd::uniform(0, 100);
        changeProb.set(rnd::uniform(0.1));
        changeDev.set(rnd::uniform(2.0));
    }

    void generateAudio(AudioIOData &io) {
        float noise;
        float max = 0.0;
        while (io()) {
            float outL, outR;
            if (rnd::prob(changeProb.get()/1000.0)) {
                freqDev1 = phsrFreq1 * changeDev.get() * 0.01 * rnd::uniform(1.0, -1.0);
                freqDev2 = phsrFreq2 * changeDev.get() * 0.01 * rnd::uniform(1.0, -1.0);
                mOsc1.freq(phsrFreq1 + freqDev1);
                mOsc2.freq(phsrFreq2 + freqDev2);
            }
            float env = al::clip((mEnvOsc1() + mEnvOsc2()), 0.0, 1.0);
            // basstone |
            float basstone = (env * 0.5) * (mOsc1() + mOsc2());


            // noise |
            float noiseOut;
            if (mTrigger()) {
                noiseHold = mNoise();
            }

            if (rnd::prob(0.0001)) {
                mNoiseEnv.reset();
            }
            noiseOut = noiseHold * mNoiseEnv() * 0.2;


            // output

            float revOutL, revOutR;
            mReverb(basstone + noiseOut, revOutL, revOutR);

            outL = mDCBlockL(revOutL + noiseOut);
            outR = mDCBlockR(revOutR + noiseOut);
            float outerenv = mEnv();
            io.out(0) = outL * mLevel * 0.3 * outerenv;
            io.out(1) = outR * mLevel * 0.3 * outerenv;

//            noise = mNoise() * mNoiseEnv();
//            for (int i = 0; i < NUM_VOICES; i++) {
//                float value = 100000 * mResonators[i](noise) * mAmplitudes[i] *  std::pow(10, (mLevel-100)/ 20.0);
//				io.out(mOutputChannel) +=  value;
//                if(value > max) {max = value;};
//			}
        }
//        if(max < 0.000001) {mDone = true;}
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

    int mOutputChannel {0};

    int mId = 0;
    bool mDone {false};
};

class ChaosSynthApp: public App
{
public:
    ChaosSynthApp(int midiChannel) : mMidiChannel(midiChannel - 1)
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
    Parameter mLevel {"Level", "", 0.5, "", 0, 1.0};
    Parameter mFundamental {"Fundamental", "", 55.0, "", 0.0, 9000.0};

    Parameter keyboardOffset {"Key Offset", "", 0.0, "", -20, 40};

    // Presets
    PresetHandler mPresetHandler {"chaosPresets"};

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
                        app->mPresetHandler.recallPreset(msg->at(1));
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
//    mLevel.setProcessingCallback([](float value, void *userData) {
//        value = std::pow(10, (value-100)/ 20.0); // db to lin ampl
//        return value;
//    }, nullptr);
//    mFundamental.set(880);
    mLevel.set(70);
}

void ChaosSynthApp::initializeGui()
{
    gui << SequencerGUI::makePresetHandlerView(mPresetHandler, 1.0, 12, 4);
    gui << mLevel; // << mFundamental;
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
    mPresetHandler << mLevel; // << mFundamental;
    mPresetHandler << synth[0].envFreq1 << synth[0].envFreq2;
    mPresetHandler << synth[0].phsrFreq1 << synth[0].phsrFreq2;
    mPresetHandler << synth[0].noiseRnd;
    mPresetHandler << synth[0].changeProb << synth[0].changeDev;

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

//void ChaosSynthApp::multiplyAmps(float factor)
//{
//    for (int i = 0; i < NUM_VOICES; i++) {
//        mAmplitudes[i].set(mAmplitudes[i].get() * factor);
//    }
//}

//void ChaosSynthApp::randomizeAmps(float max, bool sortPartials)
//{
//    if (max > 1.0) {
//        max = 1.0;
//    } else if (max < 0.01) {
//        max = 0.01;
//    }
//    srand(time(0));
//    vector<float> randomFactors(NUM_VOICES);
//    for (int i = 0; i < NUM_VOICES; i++) {
//        int random_variable = std::rand();
//        randomFactors[i] = 0.000001 + (max *random_variable/(float) RAND_MAX);
//    }
//    if (sortPartials) {
//        sort(randomFactors.begin(), randomFactors.end());
//    }
//    for (int i = 0; i < NUM_VOICES; i++) {
//        mAmplitudes[i].set(randomFactors[i]);
//    }
//}

//void ChaosSynthApp::multiplyWidths(float factor)
//{
//    for (int i = 0; i < NUM_VOICES; i++) {
//        mWidths[i].set(mWidths[i].get() * factor);
//    }
//}

//void ChaosSynthApp::randomizeWidths(float max, bool sortPartials)
//{
//    if (max > 0.002) {
//        max = 0.002;
//    } else if (max < 0.0005) {
//        max = 0.0005;
//    }
//    srand(time(0));
//    vector<float> randomFactors(NUM_VOICES);
//    for (int i = 0; i < NUM_VOICES; i++) {
//        int random_variable = std::rand();
//        randomFactors[i] = 0.0005 + (max *random_variable/(float) RAND_MAX);
//    }
//    if (sortPartials) {
//        sort(randomFactors.begin(), randomFactors.end());
//    }
//    for (int i = 0; i < NUM_VOICES; i++) {
//        mWidths[i].set(randomFactors[i] * WIDTHFACTOR);
//    }
//}

//void ChaosSynthApp::multiplyFactors(float factor)
//{
//    for (int i = 0; i < NUM_VOICES; i++) {
//        mFactors[i].set(mFactors[i].get() * factor);
//    }
//}

//void ChaosSynthApp::randomizeFactors(float max, bool sortPartials)
//{
//    if (max > 25.0) {
//        max = 25.0;
//    } else if (max < 1) {
//        max = 1.0;
//    }
//    srand(time(0));
//    vector<float> randomFactors(NUM_VOICES);
//    for (int i = 0; i < NUM_VOICES; i++) {
//        int random_variable = std::rand();
//        randomFactors[i] = 1 + (max *random_variable/(float) RAND_MAX);
//    }
//    if (sortPartials) {
//        sort(randomFactors.begin(), randomFactors.end());
//    }
//    for (int i = 0; i < NUM_VOICES; i++) {
//        mFactors[i].set(randomFactors[i]);
//    }
//}


void ChaosSynthApp::trigger(int id)
{
    ChaosSynthParameters params;
    params.id = id;
    params.mLevel = mLevel.get();
//    params.mFundamental = mFundamental.get();
//    params.mCumulativeDelay = mCumulativeDelay.get();
//    params.mCumDelayRandomness = mCumulativeDelayRandomness.get();
//    params.mArcStart = mArcStart.get();
//    params.mArcSpan = mArcSpan.get();
//    params.mOutputRouting = outputRouting;
//    params.mOutputChannel = 2;


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
//            synth[i].release(id);
            break;
//        }
    }
}

void ChaosSynthApp::triggerKey(int id)
{
    float frequency = 440;
    int keyOffset = (int) keyboardOffset.get();
//    switch(id){
//    case '1': frequency = midi2cps(50 + keyOffset);break;
//    case '2': frequency = midi2cps(51 + keyOffset);break;
//    case '3': frequency = midi2cps(52 + keyOffset);break;
//    case '4': frequency = midi2cps(53 + keyOffset);break;
//    case '5': frequency = midi2cps(54 + keyOffset);break;
//    case '6': frequency = midi2cps(55 + keyOffset);break;
//    case '7': frequency = midi2cps(56 + keyOffset);break;
//    case '8': frequency = midi2cps(57 + keyOffset);break;
//    case '9': frequency = midi2cps(58 + keyOffset);break;
//    case '0': frequency = midi2cps(59 + keyOffset);break;
//    case 'q': frequency = midi2cps(60 + keyOffset);break;
//    case 'w': frequency = midi2cps(61 + keyOffset);break;
//    case 'e': frequency = midi2cps(62 + keyOffset);break;
//    case 'r': frequency = midi2cps(63 + keyOffset);break;
//    case 't': frequency = midi2cps(64 + keyOffset);break;
//    case 'y': frequency = midi2cps(65 + keyOffset);break;
//    case 'u': frequency = midi2cps(66 + keyOffset);break;
//    case 'i': frequency = midi2cps(67 + keyOffset);break;
//    case 'o': frequency = midi2cps(68 + keyOffset);break;
//    case 'p': frequency = midi2cps(69 + keyOffset);break;
//    case 'a': frequency = midi2cps(70 + keyOffset);break;
//    case 's': frequency = midi2cps(71 + keyOffset);break;
//    case 'd': frequency = midi2cps(72 + keyOffset);break;
//    case 'f': frequency = midi2cps(73 + keyOffset);break;
//    case 'g': frequency = midi2cps(74 + keyOffset);break;
//    case 'h': frequency = midi2cps(75 + keyOffset);break;
//    case 'j': frequency = midi2cps(76 + keyOffset);break;
//    case 'k': frequency = midi2cps(77 + keyOffset);break;
//    case 'l': frequency = midi2cps(78 + keyOffset);break;
//    case ';': frequency = midi2cps(79 + keyOffset);break;
//    case 'z': frequency = midi2cps(80 + keyOffset);break;
//    case 'x': frequency = midi2cps(81 + keyOffset);break;
//    case 'c': frequency = midi2cps(82 + keyOffset);break;
//    case 'v': frequency = midi2cps(83 + keyOffset);break;
//    case 'b': frequency = midi2cps(84 + keyOffset);break;
//    case 'n': frequency = midi2cps(85 + keyOffset);break;
//    case 'm': frequency = midi2cps(86 + keyOffset);break;
//    case ',': frequency = midi2cps(87 + keyOffset);break;
//    case '.': frequency = midi2cps(88 + keyOffset);break;
//    case '/': frequency = midi2cps(89 + keyOffset);break;
//    }

//    std::cout << id << ".." << frequency << std::endl;
//    mFundamental.set(frequency);
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
