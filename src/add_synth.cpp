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

#define NUM_VOICES 24
#define SYNTH_POLYPHONY 16

class AddSynthParameters {
public:
    int id; // Instance id (e.g. MIDI note)
    float mLevel;
    float mFundamental;
    float mCumulativeDelay;
    float mCumDelayRandomness;
    float mAttackTimes[NUM_VOICES];
    float mDecayTimes[NUM_VOICES];
    float mSustainLevels[NUM_VOICES];
    float mReleaseTimes[NUM_VOICES];
    float mFrequencyFactors[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];

    float mAmpModFrequencies[NUM_VOICES];
    float mAmpModDepth[NUM_VOICES];
    float mAmpModAttack;
    float mAmpModRelease;
	float mAttackCurve;
    float mReleaseCurve;
    bool mFreqMod;

    // Spatialization
    float mArcStart;
    float mArcSpan;
    vector<int> mOutputRouting;
};

class AddSynth {
public:
    AddSynth(){
        float envLevels[6] = {0.0, 0.0, 1.0, 0.7, 0.7, 0.0};
        float ampModEnvLevels[4] = {0.0, 1.0, 0.0};
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].levels(envLevels, 6).sustainPoint(4).finish();
            mAmpModEnvelopes[i].levels(ampModEnvLevels, 3).sustainPoint(1).finish();

//			mAmpModEnvelopes[i].lengths()[1] = 1.0;
        }
        setCurvature(4);
        release();
    }

    void trigger(AddSynthParameters &params) {
        mId = params.id;
        mLevel = params.mLevel;
        updateOutMap(params.mArcStart, params.mArcSpan, params.mOutputRouting);
        memcpy(mFrequencyFactors, params.mFrequencyFactors, sizeof(float) * NUM_VOICES); // Must be called before settinf oscillator fundamental
        setOscillatorFundamental(params.mFundamental);
        setInitialCumulativeDelay(params.mCumulativeDelay, params.mCumDelayRandomness);
        memcpy(mAmplitudes, params.mAmplitudes, sizeof(float) * NUM_VOICES);
        mFreqMod = params.mFreqMod == 1.0;

		setAttackCurvature(params.mAttackCurve);
		setReleaseCurvature(params.mReleaseCurve);
        for (int i = 0; i < NUM_VOICES; i++) {
            setAttackTime(params.mAttackTimes[i], i);
            setDecayTime(params.mDecayTimes[i], i);
            setSustainLevel(params.mSustainLevels[i], i);
            setReleaseTime(params.mReleaseTimes[i], i);
            setAmpModAttackTime(params.mAmpModAttack, i);
            setAmpModReleaseTime(params.mAmpModRelease, i);
            if (mFreqMod) {
                mAmpModulators[i].set(params.mAmpModFrequencies[i] * mOscillators[i].freq(), 5 * params.mAmpModDepth[i] * mOscillators[i].freq());
            } else {
                mAmpModulators[i].set(params.mAmpModFrequencies[i], params.mAmpModDepth[i]);
            }

            mEnvelopes[i].reset();
            mAmpModEnvelopes[i].reset();
        }
    }

    void release() {
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].release();
            mAmpModEnvelopes[i].release();
        }
    }

    int id() { return mId;}

    bool done() {
        for (int i = 0; i < NUM_VOICES; i++) {
            if (!mEnvelopes[i].done()) return false;
        }
        return true;
    }

    void generateAudio(AudioIOData &io) {
        while (io()) {
            for (int i = 0; i < NUM_VOICES; i++) {
                if (mFreqMod) {
                    mOscillators[i].freq( mFundamental * mFrequencyFactors[i] + (mAmpModulators[i]() * mAmpModEnvelopes[i]()));
                    io.out(mOutMap[i]) +=  mAttenuation * mOscillators[i]() * mEnvelopes[i]() *  mAmplitudes[i] * mLevel;
                } else {
                    io.out(mOutMap[i]) +=  mAttenuation * mOscillators[i]() * mEnvelopes[i]() *  mAmplitudes[i] * mLevel
                            * (1 + mAmpModulators[i]() * mAmpModEnvelopes[i]());
                }
            }
        }
    }

    void setInitialCumulativeDelay(float initialDelay, float randomDev)
    {
        srand(time(0));
        for (int i = 0; i < NUM_VOICES; i++) {
            int random_variable = std::rand();
            float dev = randomDev *((2.0 * random_variable/(float) RAND_MAX) - 1.0);

            if (initialDelay >= 0) {
                float length = initialDelay * i + dev;
                if (length < 0) {length = 0;}
                mEnvelopes[i].lengths()[0] = length;
            } else {
                float length = -initialDelay * (NUM_VOICES - i - 1) + dev;
                if (length < 0) {length = 0;}
                mEnvelopes[i].lengths()[0] = length;
            }
        }
    }

    void setAttackTime(float attackTime, int i)
    {
        mEnvelopes[i].lengths()[1] = attackTime;
    }

    void setDecayTime(float decayTime, int i)
    {
        mEnvelopes[i].lengths()[2] = decayTime;
    }

    void setAmpModAttackTime(float attackTime, int i)
    {
        mAmpModEnvelopes[i].lengths()[0] = attackTime;
    }

    void setAmpModReleaseTime(float releaseTime, int i)
    {
        mAmpModEnvelopes[i].lengths()[1] = releaseTime;
    }

    void setSustainLevel(float sustainLevel, int i)
    {
        mEnvelopes[i].levels()[3] = sustainLevel;
        mEnvelopes[i].levels()[4] = sustainLevel;
    }

    void setReleaseTime(float releaseTime, int i)
    {
        mEnvelopes[i].lengths()[4] = releaseTime;
    }

    void setCurvature(float curvature)
    {
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].curve(curvature);
            mAmpModEnvelopes[i].curve(curvature);
        }
    }

	void setAttackCurvature(float curvature)
    {
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].curves()[1] = curvature;
            mAmpModEnvelopes[i].curves()[1] = curvature;
        }
    }

	void setReleaseCurvature(float curvature)
    {
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].curves()[4] = curvature;
            mAmpModEnvelopes[i].curves()[4] = curvature;
        }
    }

    void setOscillatorFundamental(float frequency)
    {
        mFundamental = frequency;
        for (int i = 0; i < NUM_VOICES; i++) {
            mOscillators[i].freq(frequency * mFrequencyFactors[i]);
        }
    }

    void updateOutMap(float arcStart, float arcSpan, vector<int> outputRouting) {
        int numSpeakers = outputRouting.size();
        for (int i = 0; i < NUM_VOICES; i++) {
            mOutMap[i] = outputRouting[fmod(((arcStart + (arcSpan * i/(float) (NUM_VOICES))) * numSpeakers ), numSpeakers)];
//            std::cout << mOutMap[i] << std::endl;
        }
    }

private:

    // Instance parameters

    // Synthesis
    gam::Sine<> mOscillators[NUM_VOICES];
    gam::Env<5> mEnvelopes[NUM_VOICES]; // First segment determines envelope delay
    gam::SineR<> mAmpModulators[NUM_VOICES];
    gam::Env<3> mAmpModEnvelopes[NUM_VOICES];

    bool mFreqMod;

    int mId = 0;
    float mLevel = 0;
    float mFundamental;
    float mFrequencyFactors[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];

    float mAttenuation {0.01};

    int mOutMap[NUM_VOICES] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
    int mLayer;

};

class PresetKeyboardControl;

class AddSynthApp: public App
{
public:
    AddSynthApp(int midiChannel) : /*mKeyboardPresets(nav()),*/ mMidiChannel(midiChannel - 1)
    {
        initializeValues();
        initWindow();
#ifdef SURROUND
        int outChans = 8;
        outputRouting = { {4, 3, 7, 6, 2 },
                          {4, 3, 7, 6, 2 },
                          {4, 3, 7, 6, 2 }
                        }
                          ;
#else
        int outChans = 2;
        outputRouting = {0, 1};
#endif
        initAudio(44100, 2048, outChans, 0);
        gam::sampleRate(audioIO().fps());

        AudioDevice::printAll();
        audioIO().print();

        initializePresets(); // Must be called before initializeGui
        initializeGui();

    }

    void initializeValues();
    void initializeGui();
    void initializePresets();

    virtual void onCreate(const ViewpointWindow& win) override {
        gui.setParentWindow(*windows()[0]);

        controlView.parentWindow(*windows()[0]);

        mModType.registerChangeCallback([](float value, void *sender,
                                        void *userData, void * blockSender) {
            if (static_cast<AddSynthApp *>(userData)->modDropDown != blockSender) {
                static_cast<AddSynthApp *>(userData)->modDropDown->setValue(value == 0 ? "Amp": "Freq");
            }
        }, this);

//        windows()[0]->prepend(mKeyboardPresets);
    }

    virtual void onSound(AudioIOData &io) override;

    virtual void onKeyDown(const Keyboard& k) override;
    virtual void onKeyUp(const Keyboard& k) override;

    // Transform partials
    void multiplyPartials(float factor);
    void randomizePartials(float max, bool sortPartials = true);
    void harmonicPartials();
    void oddPartials();

    // Transform amplitudes
    void setAmpsToOne();
    void setAmpsOneOverN();
    void ampSlopeFactor(double factor);

    void attackTimeAdd(float value);
    void attackTimeSlope(float factor);
    void decayTimeAdd(float value);
    void decayTimeSlope(float factor);
    void sustainAdd(float value);
    void sustainSlope(float factor);
    void releaseTimeAdd(float value);
    void releaseTimeSlope(float factor);

    void ampModFreqAdd(float value);
    void ampModFreqSlope(float factor);
	void ampModRandomize(float max, bool sortPartials = true);

    // Envelope
    void trigger(int id);
    void release(int id);
    void setInitialCumulativeDelay(float initialDelay);
    void setAttackTime(float attackTime);
    void setReleaseTime(float releaseTime);
    void setCurvature(float curvature);

private:

    // Parameters
    Parameter mLevel {"Level", "", 0.05f, "", 0.0, 1.0};
    Parameter mFundamental {"Fundamental", "", 220.0, "", 0.0, 9000.0};
    Parameter mCumulativeDelay{"CumDelay", "", 0.0, "", -0.2f, 0.2f};
    Parameter mCumulativeDelayRandomness{"CumDelayDev", "", 0.0, "", 0.0, 1.0};
	Parameter mAttackCurve{"AttackCurve", "", 4.0, "", -10.0, 10.0};
	Parameter mReleaseCurve{"ReleaseCurve", "", -4.0, "", -10.0, 10.0};
    Parameter mLayer {"Layer", "", 1.0, "", 0.0, 2.0};

    // Spatialization
    int mNumSpeakers; // Assumes regular angular separation
    Parameter mArcStart {"ArcStart", "", 0, "", 0.0, 1.0}; // 0 -> 1 (fraction of circle)
    Parameter mArcSpan {"ArcSpan", "", 0, "", 0.0, 2.0}; // 1 = full circumference. + -> CW, - -> CCW

    vector<Parameter> mAttackTimes = {
	    {"Attack1", "", 0.1f, "", 0.0, 60.0},
        {"Attack2", "", 0.1f, "", 0.0, 60.0},
        {"Attack3", "", 0.1f, "", 0.0, 60.0},
        {"Attack4", "", 0.1f, "", 0.0, 60.0},
        {"Attack5", "", 0.1f, "", 0.0, 60.0},
        {"Attack6", "", 0.1f, "", 0.0, 60.0},
        {"Attack7", "", 0.1f, "", 0.0, 60.0},
        {"Attack8", "", 0.1f, "", 0.0, 60.0},
        {"Attack9", "", 0.1f, "", 0.0, 60.0},
        {"Attack10", "", 0.1f, "", 0.0, 60.0},
        {"Attack11", "", 0.1f, "", 0.0, 60.0},
        {"Attack12", "", 0.1f, "", 0.0, 60.0},
        {"Attack13", "", 0.1f, "", 0.0, 60.0},
        {"Attack14", "", 0.1f, "", 0.0, 60.0},
        {"Attack15", "", 0.1f, "", 0.0, 60.0},
        {"Attack16", "", 0.1f, "", 0.0, 60.0},
        {"Attack17", "", 0.1f, "", 0.0, 60.0},
        {"Attack18", "", 0.1f, "", 0.0, 60.0},
        {"Attack19", "", 0.1f, "", 0.0, 60.0},
        {"Attack20", "", 0.1f, "", 0.0, 60.0},
        {"Attack21", "", 0.1f, "", 0.0, 60.0},
        {"Attack22", "", 0.1f, "", 0.0, 60.0},
        {"Attack23", "", 0.1f, "", 0.0, 60.0},
        {"Attack24", "", 0.1f, "", 0.0, 60.0}
                                              };
    vector<Parameter> mDecayTimes = {
        {"Decay1", "", 0.1f, "", 0.0, 20.0},
        {"Decay2", "", 0.1f, "", 0.0, 20.0},
        {"Decay3", "", 0.1f, "", 0.0, 20.0},
        {"Decay4", "", 0.1f, "", 0.0, 20.0},
        {"Decay5", "", 0.1f, "", 0.0, 20.0},
        {"Decay6", "", 0.1f, "", 0.0, 20.0},
        {"Decay7", "", 0.1f, "", 0.0, 20.0},
        {"Decay8", "", 0.1f, "", 0.0, 20.0},
        {"Decay9", "", 0.1f, "", 0.0, 20.0},
        {"Decay10", "", 0.1f, "", 0.0, 20.0},
        {"Decay11", "", 0.1f, "", 0.0, 20.0},
        {"Decay12", "", 0.1f, "", 0.0, 20.0},
        {"Decay13", "", 0.1f, "", 0.0, 20.0},
        {"Decay14", "", 0.1f, "", 0.0, 20.0},
        {"Decay15", "", 0.1f, "", 0.0, 20.0},
        {"Decay16", "", 0.1f, "", 0.0, 20.0},
        {"Decay17", "", 0.1f, "", 0.0, 20.0},
        {"Decay18", "", 0.1f, "", 0.0, 20.0},
        {"Decay19", "", 0.1f, "", 0.0, 20.0},
        {"Decay20", "", 0.1f, "", 0.0, 20.0},
        {"Decay21", "", 0.1f, "", 0.0, 20.0},
        {"Decay22", "", 0.1f, "", 0.0, 20.0},
        {"Decay23", "", 0.1f, "", 0.0, 20.0},
        {"Decay24", "", 0.1f, "", 0.0, 20.0}
                                              };
    vector<Parameter> mSustainLevels = {
        {"Sustain1", "", 0.7f, "", 0.0, 1.0},
        {"Sustain2", "", 0.7f, "", 0.0, 1.0},
        {"Sustain3", "", 0.7f, "", 0.0, 1.0},
        {"Sustain4", "", 0.7f, "", 0.0, 1.0},
        {"Sustain5", "", 0.7f, "", 0.0, 1.0},
        {"Sustain6", "", 0.7f, "", 0.0, 1.0},
        {"Sustain7", "", 0.7f, "", 0.0, 1.0},
        {"Sustain8", "", 0.7f, "", 0.0, 1.0},
        {"Sustain9", "", 0.7f, "", 0.0, 1.0},
        {"Sustain10", "", 0.7f, "", 0.0, 1.0},
        {"Sustain11", "", 0.7f, "", 0.0, 1.0},
        {"Sustain12", "", 0.7f, "", 0.0, 1.0},
        {"Sustain13", "", 0.7f, "", 0.0, 1.0},
        {"Sustain14", "", 0.7f, "", 0.0, 1.0},
        {"Sustain15", "", 0.7f, "", 0.0, 1.0},
        {"Sustain16", "", 0.7f, "", 0.0, 1.0},
        {"Sustain17", "", 0.7f, "", 0.0, 1.0},
        {"Sustain18", "", 0.7f, "", 0.0, 1.0},
        {"Sustain19", "", 0.7f, "", 0.0, 1.0},
        {"Sustain20", "", 0.7f, "", 0.0, 1.0},
        {"Sustain21", "", 0.7f, "", 0.0, 1.0},
        {"Sustain22", "", 0.7f, "", 0.0, 1.0},
        {"Sustain23", "", 0.7f, "", 0.0, 1.0},
        {"Sustain24", "", 0.7f, "", 0.0, 1.0}
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
        {"Release16", "", 1.0f, "", 0.0, 20.0},
        {"Release17", "", 1.0f, "", 0.0, 20.0},
        {"Release18", "", 1.0f, "", 0.0, 20.0},
        {"Release19", "", 1.0f, "", 0.0, 20.0},
        {"Release20", "", 1.0f, "", 0.0, 20.0},
        {"Release21", "", 1.0f, "", 0.0, 20.0},
        {"Release22", "", 1.0f, "", 0.0, 20.0},
        {"Release23", "", 1.0f, "", 0.0, 20.0},
        {"Release24", "", 1.0f, "", 0.0, 20.0}
                                              };

    vector<Parameter> mFrequencyFactors = {
        {"Harm1", "", 1.0f, "", 0.0, 30.0},
        {"Harm2", "", 2.0f, "", 0.0, 30.0},
        {"Harm3", "", 3.0f, "", 0.0, 30.0},
        {"Harm4", "", 4.0f, "", 0.0, 30.0},
        {"Harm5", "", 5.0f, "", 0.0, 30.0},
        {"Harm6", "", 6.0f, "", 0.0, 30.0},
        {"Harm7", "", 7.0f, "", 0.0, 30.0},
        {"Harm8", "", 8.0f, "", 0.0, 30.0},
        {"Harm9", "", 9.0f, "", 0.0, 30.0},
        {"Harm10", "", 10.0f, "", 0.0, 30.0},
        {"Harm11", "", 11.0f, "", 0.0, 30.0},
        {"Harm12", "", 12.0f, "", 0.0, 30.0},
        {"Harm13", "", 13.0f, "", 0.0, 30.0},
        {"Harm14", "", 14.0f, "", 0.0, 30.0},
        {"Harm15", "", 15.0f, "", 0.0, 30.0},
        {"Harm16", "", 16.0f, "", 0.0, 30.0},
        {"Harm17", "", 17.0f, "", 0.0, 30.0},
        {"Harm18", "", 10.0f, "", 0.0, 30.0},
        {"Harm19", "", 11.0f, "", 0.0, 30.0},
        {"Harm20", "", 12.0f, "", 0.0, 30.0},
        {"Harm21", "", 13.0f, "", 0.0, 30.0},
        {"Harm22", "", 14.0f, "", 0.0, 30.0},
        {"Harm23", "", 15.0f, "", 0.0, 30.0},
        {"Harm24", "", 16.0f, "", 0.0, 30.0}
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
        {"Amp16", "", 1/16.0f, "", 0.0, 1.0},
        {"Amp17", "", 1/9.0f, "", 0.0, 1.0},
        {"Amp18", "", 1/10.0f, "", 0.0, 1.0},
        {"Amp19", "", 1/11.0f, "", 0.0, 1.0},
        {"Amp20", "", 1/12.0f, "", 0.0, 1.0},
        {"Amp21", "", 1/13.0f, "", 0.0, 1.0},
        {"Amp22", "", 1/14.0f, "", 0.0, 1.0},
        {"Amp23", "", 1/15.0f, "", 0.0, 1.0},
        {"Amp24", "", 1/16.0f, "", 0.0, 1.0}
                                              };
    // Amp Mod
    Parameter mModType{"modType", "", 0.0, "", 0.0, 1.0};
    Parameter mModDepth{"modDepth", "", 0.0, "", 0.0, 1.0};
    Parameter mModAttack{"modAttack", "", 0.0, "", 0.0, 10.0};
    Parameter mModRelease{"modRelease", "", 5.0, "", 0.0, 10.0};

    vector<Parameter> mAmpModFrequencies = {
        {"modFreq1", "", 1.0f, "", 0.0, 30.0},
        {"modFreq2", "", 1.0f, "", 0.0, 30.0},
        {"modFreq3", "", 1.0f, "", 0.0, 30.0},
        {"modFreq4", "", 1.0f, "", 0.0, 30.0},
        {"modFreq5", "", 1.0f, "", 0.0, 30.0},
        {"modFreq6", "", 1.0f, "", 0.0, 30.0},
        {"modFreq7", "", 1.0f, "", 0.0, 30.0},
        {"modFreq8", "", 1.0f, "", 0.0, 30.0},
        {"modFreq9", "", 1.0f, "", 0.0, 30.0},
        {"modFreq10", "",1.0f, "", 0.0, 30.0},
        {"modFreq11", "",1.0f, "", 0.0, 30.0},
        {"modFreq12", "",1.0f, "", 0.0, 30.0},
        {"modFreq13", "",1.0f, "", 0.0, 30.0},
        {"modFreq14", "",1.0f, "", 0.0, 30.0},
        {"modFreq15", "",1.0f, "", 0.0, 30.0},
        {"modFreq16", "",1.0f, "", 0.0, 30.0},
        {"modFreq17", "",1.0f, "", 0.0, 30.0},
        {"modFreq18", "",1.0f, "", 0.0, 30.0},
        {"modFreq19", "",1.0f, "", 0.0, 30.0},
        {"modFreq20", "",1.0f, "", 0.0, 30.0},
        {"modFreq21", "",1.0f, "", 0.0, 30.0},
        {"modFreq22", "",1.0f, "", 0.0, 30.0},
        {"modFreq23", "",1.0f, "", 0.0, 30.0},
        {"modFreq24", "",1.0f, "", 0.0, 30.0}
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
    glv::Box decayTimesBox {glv::Direction::S};
    glv::Box sustainLevelsBox {glv::Direction::S};
    glv::Box releaseTimesBox {glv::Direction::S};
    glv::Box ampModBox {glv::Direction::S};
    glv::DropDown *modDropDown;
    GLVDetachable controlView;

//    PresetKeyboardControl mKeyboardPresets;
    bool mPresetKeyboardActive = true;

    // MIDI Control
    PresetMIDI presetMIDI;
    ParameterMIDI parameterMIDI;

    ParameterBool midiLight{"MIDI", "", false};

    MIDIIn midiIn {"USB Oxygen 49"};

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

//    static void midiLightWaiter(AddSynthApp *app) {
//        static std::mutex midiLightLock;
//        static std::shared_ptr<std::thread> th;
//        static std::atomic<bool> done(true); // Use an atomic flag.
//        if(midiLightLock.try_lock()) {
//            if (done.load()) {
//               if(th) {
//                   th->join();
//               }
//               done.store(false);
//               th = std::make_shared<std::thread>([](AddSynthApp *app) {
//                   std::this_thread::sleep_for(std::chrono::milliseconds(500));
//                   app->midiLight.set(false);
//                   done.store(true);
//               },
//               app
//               );
//               midiLightLock.unlock();
//            }
//        }
//    }

    static void midiCallback(double deltaTime, std::vector<unsigned char> *msg, void *userData){
        AddSynthApp *app = static_cast<AddSynthApp *>(userData);
        unsigned numBytes = msg->size();
//        midiLightWaiter(app);

        if(numBytes > 0){
            unsigned char status = msg->at(0);
            if(MIDIByte::isChannelMessage(status)){
                unsigned char type = status & MIDIByte::MESSAGE_MASK;
                unsigned char chan = status & MIDIByte::CHANNEL_MASK;
                if ((int) chan == app->mMidiChannel) {
                    app->midiLight.set(true);
                    switch(type){
                    case MIDIByte::NOTE_ON:
                        //                    printf("Note %u, Vel %u", msg->at(1), msg->at(2));
                        if (msg->at(2) != 0) {
                            app->mFundamental.set(midi2cps(msg->at(1)));
                            app->trigger(msg->at(1));
                        } else {
                            app->release(msg->at(1));
                        }
                        break;

                    case MIDIByte::NOTE_OFF:

                        app->release(msg->at(1));
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
    int mMidiChannel;
    AddSynth synth[SYNTH_POLYPHONY];
    vector<vector<int>> outputRouting;
};

class PresetKeyboardControl : public NavInputControl {
public:

	PresetKeyboardControl (Nav &nav, AddSynthApp *app) : NavInputControl(nav), mApp(app){ mUseMouse = false;}

	virtual bool onKeyDown(const Keyboard &k) {
		if (k.ctrl() || k.alt() || k.meta()) {
			return true;
		}
//		if (!mApp->mPresetKeyboardActive) {
//			return true;
//		}
		switch(k.key()){
		case '1': mApp->trigger(0); return false;
		case '2': mApp->trigger(1); return false;
		case '3': mApp->trigger(2); return false;
		case '4': mApp->trigger(3); return false;
		case '5': mApp->trigger(4); return false;
		case '6': mApp->trigger(5); return false;
		case '7': mApp->trigger(6); return false;
		case '8': mApp->trigger(7); return false;
		case '9': mApp->trigger(8); return false;
		case '0': mApp->trigger(9); return false;
		case 'q': mApp->trigger(10); return false;
		case 'w': mApp->trigger(11); return false;
		case 'e': mApp->trigger(12); return false;
		case 'r': mApp->trigger(13); return false;
		case 't': mApp->trigger(14); return false;
		case 'y': mApp->trigger(15); return false;
		case 'u': mApp->trigger(16); return false;
		case 'i': mApp->trigger(17); return false;
		case 'o': mApp->trigger(18); return false;
		case 'p': mApp->trigger(19); return false;
		case 'a': mApp->trigger(20); return false;
		case 's': mApp->trigger(21); return false;
		case 'd': mApp->trigger(22); return false;
		case 'f': mApp->trigger(23); return false;
		case 'g': mApp->trigger(24); return false;
		case 'h': mApp->trigger(25); return false;
		case 'j': mApp->trigger(26); return false;
		case 'k': mApp->trigger(27); return false;
		case 'l': mApp->trigger(28); return false;
		case ';': mApp->trigger(29); return false;
		case 'z': mApp->trigger(30); return false;
		case 'x': mApp->trigger(31); return false;
		case 'c': mApp->trigger(32); return false;
		case 'v': mApp->trigger(33); return false;
		case 'b': mApp->trigger(34); return false;
		case 'n': mApp->trigger(35); return false;
		case 'm': mApp->trigger(36); return false;
		case ',': mApp->trigger(37); return false;
		case '.': mApp->trigger(38); return false;
		case '/': mApp->trigger(39); return false;
		default: break;
		}
		return true;
	}

	virtual bool onKeyUp(const Keyboard &k) {
        if (k.ctrl() || k.alt() || k.meta()) {
			return true;
		}
//		if (!mApp->mPresetKeyboardActive) {
//			return true;
//		}
		switch(k.key()){
		case '1': mApp->release(0); return false;
		case '2': mApp->release(1); return false;
		case '3': mApp->release(2); return false;
		case '4': mApp->release(3); return false;
		case '5': mApp->release(4); return false;
		case '6': mApp->release(5); return false;
		case '7': mApp->release(6); return false;
		case '8': mApp->release(7); return false;
		case '9': mApp->release(8); return false;
		case '0': mApp->release(9); return false;
		case 'q': mApp->release(10); return false;
		case 'w': mApp->release(11); return false;
		case 'e': mApp->release(12); return false;
		case 'r': mApp->release(13); return false;
		case 't': mApp->release(14); return false;
		case 'y': mApp->release(15); return false;
		case 'u': mApp->release(16); return false;
		case 'i': mApp->release(17); return false;
		case 'o': mApp->release(18); return false;
		case 'p': mApp->release(19); return false;
		case 'a': mApp->release(20); return false;
		case 's': mApp->release(21); return false;
		case 'd': mApp->release(22); return false;
		case 'f': mApp->release(23); return false;
		case 'g': mApp->release(24); return false;
		case 'h': mApp->release(25); return false;
		case 'j': mApp->release(26); return false;
		case 'k': mApp->release(27); return false;
		case 'l': mApp->release(28); return false;
		case ';': mApp->release(29); return false;
		case 'z': mApp->release(30); return false;
		case 'x': mApp->release(31); return false;
		case 'c': mApp->release(32); return false;
		case 'v': mApp->release(33); return false;
		case 'b': mApp->release(34); return false;
		case 'n': mApp->release(35); return false;
		case 'm': mApp->release(36); return false;
		case ',': mApp->release(37); return false;
		case '.': mApp->release(38); return false;
		case '/': mApp->release(39); return false;
		default: break;
		}
		return true;
	}
    PresetHandler *presets;
    AddSynthApp *mApp;
};



// ----------------------- Implementations



void AddSynthApp::initializeValues()
{
    mFundamental.set(220);
    harmonicPartials();

    mLevel.set(0.5);
    mCumulativeDelay.set(0.0);
    for (int i = 0; i < NUM_VOICES; i++) {
        mAttackTimes[i].set(0.1);
        mDecayTimes[i].set(0.1);
        mSustainLevels[i].set(0.7);
        mReleaseTimes[i].set(2.0);
    }
}

void AddSynthApp::initializeGui()
{
    gui << SequencerGUI::makePresetHandlerView(mPresetHandler, 1.0, 12, 4);
    gui << mLevel << mFundamental << mCumulativeDelay << mCumulativeDelayRandomness;
    gui << mArcStart << mArcSpan;
	gui << mAttackCurve << mReleaseCurve;
    gui << mLayer;
    gui << midiLight;

    glv::Button *keyboardButton = new glv::Button;
    glv::Table *table = new glv::Table("><");
    *table << keyboardButton << new glv::Label("Keyboard");
    table->arrange();
    gui << table;
    keyboardButton->attachVariable(&mPresetKeyboardActive, 1);

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
        if (b->getValue() == 1) {
            app->multiplyPartials(0.99);
        }
    }, glv::Update::Value, this);
    compressButton->property(glv::Momentary, true);
    harmonicsBox << compressButton << new glv::Label("Compress");

    glv::Button *expandButton = new glv::Button;
    expandButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->multiplyPartials(1/0.99);
        }
    }, glv::Update::Value, this);
    expandButton->property(glv::Momentary, true);
    harmonicsBox << expandButton << new glv::Label("Expand");

    glv::Button *randomizeButton = new glv::Button;
    randomizeButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->randomizePartials(10.0);
        }
    }, glv::Update::Value, this);
    randomizeButton->property(glv::Momentary, true);
    harmonicsBox << randomizeButton << new glv::Label("Randomize Sort");

    glv::Button *randomizeNoSortButton = new glv::Button;
    randomizeNoSortButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->randomizePartials(10.0, false);
        }
    }, glv::Update::Value, this);
    randomizeNoSortButton->property(glv::Momentary, true);
    harmonicsBox << randomizeNoSortButton << new glv::Label("Randomize");

    glv::Button *harmonicButton = new glv::Button;
    harmonicButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->harmonicPartials();
        }
    }, glv::Update::Value, this);
    harmonicButton->property(glv::Momentary, true);
    harmonicsBox << harmonicButton << new glv::Label("Harmonic");

    glv::Button *evenButton = new glv::Button;
    evenButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->oddPartials();
        }
    }, glv::Update::Value, this);
    evenButton->property(glv::Momentary, true);
    harmonicsBox << evenButton << new glv::Label("Even only");

    for (int i = 0; i < NUM_VOICES; i++) {
        harmonicsBox << ParameterGUI::makeParameterView(mFrequencyFactors[i]);
    }
    harmonicsBox.fit();
    harmonicsBox.enable(glv::DrawBack);
    harmonicsBox.anchor(glv::Place::TR);
    harmonicsBox.set(-1300, 30, harmonicsBox.width(), harmonicsBox.height());
    controlView << harmonicsBox;

    // Amplitudes

    glv::Button *oneButton = new glv::Button;
    oneButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->setAmpsToOne();
        }
    }, glv::Update::Value, this);
    oneButton->property(glv::Momentary, true);
    amplitudesBox << oneButton << new glv::Label("All One");

    glv::Button *oneOverNButton = new glv::Button;
    oneOverNButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->setAmpsOneOverN();
        }
    }, glv::Update::Value, this);
    oneOverNButton->property(glv::Momentary, true);
    amplitudesBox << oneOverNButton << new glv::Label("One over N");

    glv::Button *slopeUpButton = new glv::Button;
    slopeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampSlopeFactor(1.005);
        }
    }, glv::Update::Value, this);
    slopeUpButton->property(glv::Momentary, true);
    amplitudesBox << slopeUpButton << new glv::Label("Slope up");

    glv::Button *slopeDownButton = new glv::Button;
    slopeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampSlopeFactor(1/1.005);
        }
    }, glv::Update::Value, this);
    slopeDownButton->property(glv::Momentary, true);
    amplitudesBox << slopeDownButton << new glv::Label("Slope down");

    for (int i = 0; i < NUM_VOICES; i++) {
        amplitudesBox << ParameterGUI::makeParameterView(mAmplitudes[i]);
    }
    amplitudesBox.fit();
    amplitudesBox.enable(glv::DrawBack);
    amplitudesBox.anchor(glv::Place::TR);
    amplitudesBox.set(-1150, 30, amplitudesBox.width(), amplitudesBox.height());
    controlView << amplitudesBox;

    // Attack Times

    glv::Button *attackTimeUpButton = new glv::Button;
    attackTimeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->attackTimeAdd(0.05);
        }
    }, glv::Update::Value, this);
    attackTimeUpButton->property(glv::Momentary, true);
    attackTimesBox << attackTimeUpButton << new glv::Label("Slower Attack");

    glv::Button *attackTimeDownButton = new glv::Button;
    attackTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->attackTimeAdd(-0.05);
        }
    }, glv::Update::Value, this);
    attackTimeDownButton->property(glv::Momentary, true);
    attackTimesBox << attackTimeDownButton << new glv::Label("Faster Attack");

    glv::Button *attackRampUpButton = new glv::Button;
    attackRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->attackTimeSlope(1.01);
        }
    }, glv::Update::Value, this);
    attackRampUpButton->property(glv::Momentary, true);
    attackTimesBox << attackRampUpButton << new glv::Label("Ramp up");

    glv::Button *attackRampDownButton = new glv::Button;
    attackRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->attackTimeSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    attackRampDownButton->property(glv::Momentary, true);
    attackTimesBox << attackRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        attackTimesBox << ParameterGUI::makeParameterView(mAttackTimes[i]);
    }
    attackTimesBox.fit();
    attackTimesBox.enable(glv::DrawBack);
    attackTimesBox.anchor(glv::Place::TR);
    attackTimesBox.set(-1000, 30, attackTimesBox.width(), attackTimesBox.height());
    controlView << attackTimesBox;

    // Decay Times

    glv::Button *decayTimeUpButton = new glv::Button;
    decayTimeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->decayTimeAdd(0.05);
        }
    }, glv::Update::Value, this);
    decayTimeUpButton->property(glv::Momentary, true);
    decayTimesBox << decayTimeUpButton << new glv::Label("Slower Decay");

    glv::Button *decayTimeDownButton = new glv::Button;
    decayTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->decayTimeAdd(-0.05);
        }
    }, glv::Update::Value, this);
    decayTimeDownButton->property(glv::Momentary, true);
    decayTimesBox << decayTimeDownButton << new glv::Label("Faster Decay");

    glv::Button *decayRampUpButton = new glv::Button;
    decayRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->decayTimeSlope(1.01);
        }
    }, glv::Update::Value, this);
    decayRampUpButton->property(glv::Momentary, true);
    decayTimesBox << decayRampUpButton << new glv::Label("Ramp up");

    glv::Button *decayRampDownButton = new glv::Button;
    decayRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->decayTimeSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    decayRampDownButton->property(glv::Momentary, true);
    decayTimesBox << decayRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        decayTimesBox << ParameterGUI::makeParameterView(mDecayTimes[i]);
    }
    decayTimesBox.fit();
    decayTimesBox.enable(glv::DrawBack);
    decayTimesBox.anchor(glv::Place::TR);
    decayTimesBox.set(-850, 30, decayTimesBox.width(), decayTimesBox.height());
    controlView << decayTimesBox;

    // Sustain

    glv::Button *sustainTimeUpButton = new glv::Button;
    sustainTimeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->sustainAdd(0.05);
        }
    }, glv::Update::Value, this);
    sustainTimeUpButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainTimeUpButton << new glv::Label("Higher Sus");

    glv::Button *sustainTimeDownButton = new glv::Button;
    sustainTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->sustainAdd(-0.05);
        }
    }, glv::Update::Value, this);
    sustainTimeDownButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainTimeDownButton << new glv::Label("Lower Sus");

    glv::Button *sustainRampUpButton = new glv::Button;
    sustainRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->sustainSlope(1.01);
        }
    }, glv::Update::Value, this);
    sustainRampUpButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainRampUpButton << new glv::Label("Ramp up");

    glv::Button *sustainRampDownButton = new glv::Button;
    sustainRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->sustainSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    sustainRampDownButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        sustainLevelsBox << ParameterGUI::makeParameterView(mSustainLevels[i]);
    }
    sustainLevelsBox.fit();
    sustainLevelsBox.enable(glv::DrawBack);
    sustainLevelsBox.anchor(glv::Place::TR);
    sustainLevelsBox.set(-700, 30, sustainLevelsBox.width(), sustainLevelsBox.height());
    controlView << sustainLevelsBox;

    // Release Time

    glv::Button *releaseTimeUpButton = new glv::Button;
    releaseTimeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->releaseTimeAdd(0.05);
        }
    }, glv::Update::Value, this);
    releaseTimeUpButton->property(glv::Momentary, true);
    releaseTimesBox << releaseTimeUpButton << new glv::Label("Slower Release");

    glv::Button *releaseTimeDownButton = new glv::Button;
    releaseTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->releaseTimeAdd(-0.05);
        }
    }, glv::Update::Value, this);
    releaseTimeDownButton->property(glv::Momentary, true);
    releaseTimesBox << releaseTimeDownButton << new glv::Label("Faster Release");

    glv::Button *releaseRampUpButton = new glv::Button;
    releaseRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->releaseTimeSlope(1.01);
        }
    }, glv::Update::Value, this);
    releaseRampUpButton->property(glv::Momentary, true);
    releaseTimesBox << releaseRampUpButton << new glv::Label("Ramp up");

    glv::Button *releaseRampDownButton = new glv::Button;
    releaseRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->releaseTimeSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    releaseRampDownButton->property(glv::Momentary, true);
    releaseTimesBox << releaseRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        releaseTimesBox << ParameterGUI::makeParameterView(mReleaseTimes[i]);
    }
    releaseTimesBox.fit();
    releaseTimesBox.enable(glv::DrawBack);
    releaseTimesBox.anchor(glv::Place::TR);
    releaseTimesBox.set(-550, 30, releaseTimesBox.width(), releaseTimesBox.height());
    controlView << releaseTimesBox;

    // Amp Mod
    modDropDown = new glv::DropDown;
    modDropDown->addItem("Amp");
    modDropDown->addItem("Freq");
    modDropDown->attach([](const glv::Notification &n) {
        glv::DropDown *b = n.sender<glv::DropDown>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        app->mModType.setNoCalls(b->selectedItem(), b);
    }, glv::Update::Value, this);
    ampModBox << modDropDown << new glv::Label("Mod type");

    glv::Button *ampModUpButton = new glv::Button;
    ampModUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampModFreqAdd(0.05);
        }
    }, glv::Update::Value, this);
    ampModUpButton->property(glv::Momentary, true);
    ampModBox << ampModUpButton << new glv::Label("Higher freq");

    glv::Button *ampModDownButton = new glv::Button;
    ampModDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampModFreqAdd(-0.05);
        }
    }, glv::Update::Value, this);
    ampModDownButton->property(glv::Momentary, true);
    ampModBox << ampModDownButton << new glv::Label("Lower freq");

    glv::Button *ampModRampUpButton = new glv::Button;
    ampModRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampModFreqSlope(1.01);
        }
    }, glv::Update::Value, this);
    ampModRampUpButton->property(glv::Momentary, true);
    ampModBox << ampModRampUpButton << new glv::Label("Ramp up");

    glv::Button *ampModRampDownButton = new glv::Button;
    ampModRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampModFreqSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    ampModRampDownButton->property(glv::Momentary, true);
    ampModBox << ampModRampDownButton << new glv::Label("Ramp down");

    ampModBox << ParameterGUI::makeParameterView(mModDepth);
    ampModBox << ParameterGUI::makeParameterView(mModAttack);
    ampModBox << ParameterGUI::makeParameterView(mModRelease);

	glv::Button *randomizeAmpModButton = new glv::Button;
    randomizeAmpModButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->ampModRandomize(10.0, false);
        }
    }, glv::Update::Value, this);
    randomizeAmpModButton->property(glv::Momentary, true);
    ampModBox << randomizeAmpModButton << new glv::Label("Randomize Sort");

    for (int i = 0; i < NUM_VOICES; i++) {
        ampModBox << ParameterGUI::makeParameterView(mAmpModFrequencies[i]);
    }
    ampModBox.fit();
    ampModBox.enable(glv::DrawBack);
    ampModBox.anchor(glv::Place::TR);
    ampModBox.set(-400, 30, ampModBox.width(), ampModBox.height());
    controlView << ampModBox;


    controlView.set(300, 0, 300, 400);
}

void AddSynthApp::initializePresets()
{
    mPresetHandler << mLevel;
    mPresetHandler << mFundamental << mCumulativeDelay << mCumulativeDelayRandomness;
    mPresetHandler << mArcStart << mArcSpan;
    mPresetHandler << mAttackCurve << mReleaseCurve;
    mPresetHandler << mModDepth << mModAttack << mModRelease;
    mPresetHandler << mModType;
    mPresetHandler << mLayer;

    for (int i = 0; i < NUM_VOICES; i++) {
        mPresetHandler << mFrequencyFactors[i] << mAmplitudes[i];
        mPresetHandler << mAttackTimes[i] << mDecayTimes[i] << mSustainLevels[i] << mReleaseTimes[i];
        mPresetHandler << mAmpModFrequencies[i];
    }
//    mPresetHandler.print();
    sequencer << mPresetHandler;
    recorder << mPresetHandler;
//    mKeyboardPresets.presets = &mPresetHandler;

    // MIDI Control of parameters
    int midiPort = 0;
    parameterMIDI.init(midiPort);
    parameterMIDI.connectControl(mCumulativeDelay, 75, 1);
    parameterMIDI.connectControl(mCumulativeDelayRandomness, 76, 1);
    parameterMIDI.connectControl(mArcStart, 77, 1);
    parameterMIDI.connectControl(mArcSpan, 78, 1);

    // MIDI control of presets
    // 74 71 91 93 73 72 5 84 7
    // 75 76 77 78 74 71 24 102
    presetMIDI.init(midiPort, mPresetHandler);
    presetMIDI.setMorphControl(102, 1, 0.0, 8.0);
    // MIDI preset mapping
//    presetMIDI.connectNoteToPreset(1, 0, 36, 24, 59);

    // Print out names of available input ports
    for(unsigned i=0; i< midiIn.getPortCount(); ++i){
        printf("Port %u: %s\n", i, midiIn.getPortName(i).c_str());
    }
    try {
        // Open the port specified above
        midiIn.openPort(midiPort);
    }
    catch(MIDIError &error){
        error.printMessage();
    }

    midiIn.setCallback(AddSynthApp::midiCallback, this);
}

void AddSynthApp::onSound(AudioIOData &io)
{
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        if (!synth[i].done()) {
            synth[i].generateAudio(io);
            io.frame(0);
        }
    }
}

void AddSynthApp::onKeyDown(const Keyboard &k)
{
    if (!mPresetKeyboardActive) {
        return;
    }
    if (k.ctrl() || k.alt() || k.meta()) {
        trigger(100);
    }
//    switch(k.key()){
//    case '1': trigger(40);
//    case '2': trigger(41);
//    case '3': trigger(42);
//    case '4': trigger(43);
//    case '5': trigger(44);
//    case '6': trigger(45);
//    case '7': trigger(46);
//    case '8': trigger(47);
//    case '9': trigger(48);
//    case '0': trigger(49);
//    case 'q': trigger(50);
//    case 'w': trigger(51);
//    case 'e': trigger(52);
//    case 'r': trigger(53);
//    case 't': trigger(54);
//    case 'y': trigger(55);
//    case 'u': trigger(56);
//    case 'i': trigger(57);
//    case 'o': trigger(58);
//    case 'p': trigger(59);
//    case 'a': trigger(60);
//    case 's': trigger(61);
//    case 'd': trigger(62);
//    case 'f': trigger(63);
//    case 'g': trigger(64);
//    case 'h': trigger(65);
//    case 'j': trigger(66);
//    case 'k': trigger(67);
//    case 'l': trigger(68);
//    case ';': trigger(69);
//    case 'z': trigger(70);
//    case 'x': trigger(71);
//    case 'c': trigger(72);
//    case 'v': trigger(73);
//    case 'b': trigger(74);
//    case 'n': trigger(75);
//    case 'm': trigger(76);
//    case ',': trigger(77);
//    case '.': trigger(78);
//    case '/': trigger(79);
//    default: break;
//    }

    switch(k.key()){
    case '1': mPresetHandler.recallPreset(0) ; trigger(0);  break;
    case '2': mPresetHandler.recallPreset(1) ; trigger(1);  break;
    case '3': mPresetHandler.recallPreset(2) ; trigger(2);  break;
    case '4': mPresetHandler.recallPreset(3) ; trigger(3);  break;
    case '5': mPresetHandler.recallPreset(4) ; trigger(4);  break;
    case '6': mPresetHandler.recallPreset(5) ; trigger(5);  break;
    case '7': mPresetHandler.recallPreset(6) ; trigger(6);  break;
    case '8': mPresetHandler.recallPreset(7) ; trigger(7);  break;
    case '9': mPresetHandler.recallPreset(8) ; trigger(8);  break;
    case '0': mPresetHandler.recallPreset(9) ; trigger(9);  break;
    case 'q': mPresetHandler.recallPreset(10); trigger(10); break;
    case 'w': mPresetHandler.recallPreset(11); trigger(11); break;
    case 'e': mPresetHandler.recallPreset(12); trigger(12); break;
    case 'r': mPresetHandler.recallPreset(13); trigger(13); break;
    case 't': mPresetHandler.recallPreset(14); trigger(14); break;
    case 'y': mPresetHandler.recallPreset(15); trigger(15); break;
    case 'u': mPresetHandler.recallPreset(16); trigger(16); break;
    case 'i': mPresetHandler.recallPreset(17); trigger(17); break;
    case 'o': mPresetHandler.recallPreset(18); trigger(18); break;
    case 'p': mPresetHandler.recallPreset(19); trigger(19); break;
    case 'a': mPresetHandler.recallPreset(20); trigger(20); break;
    case 's': mPresetHandler.recallPreset(21); trigger(21); break;
    case 'd': mPresetHandler.recallPreset(22); trigger(22); break;
    case 'f': mPresetHandler.recallPreset(23); trigger(23); break;
    case 'g': mPresetHandler.recallPreset(24); trigger(24); break;
    case 'h': mPresetHandler.recallPreset(25); trigger(25); break;
    case 'j': mPresetHandler.recallPreset(26); trigger(26); break;
    case 'k': mPresetHandler.recallPreset(27); trigger(27); break;
    case 'l': mPresetHandler.recallPreset(28); trigger(28); break;
    case ';': mPresetHandler.recallPreset(29); trigger(29); break;
    case 'z': mPresetHandler.recallPreset(30); trigger(30); break;
    case 'x': mPresetHandler.recallPreset(31); trigger(31); break;
    case 'c': mPresetHandler.recallPreset(32); trigger(32); break;
    case 'v': mPresetHandler.recallPreset(33); trigger(33); break;
    case 'b': mPresetHandler.recallPreset(34); trigger(34); break;
    case 'n': mPresetHandler.recallPreset(35); trigger(35); break;
    case 'm': mPresetHandler.recallPreset(36); trigger(36); break;
    case ',': mPresetHandler.recallPreset(37); trigger(37); break;
    case '.': mPresetHandler.recallPreset(38); trigger(38); break;
    case '/': mPresetHandler.recallPreset(39); trigger(39); break;
    }
}

void AddSynthApp::onKeyUp(const Keyboard &k)
{
    if (!mPresetKeyboardActive) {
        return;
    }
    if (k.ctrl() || k.alt() || k.meta()) {
        release(100);
    }
    switch(k.key()){
    case '1': release(0);
    case '2': release(1);
    case '3': release(2);
    case '4': release(3);
    case '5': release(4);
    case '6': release(5);
    case '7': release(6);
    case '8': release(7);
    case '9': release(8);
    case '0': release(9);
    case 'q': release(10);
    case 'w': release(11);
    case 'e': release(12);
    case 'r': release(13);
    case 't': release(14);
    case 'y': release(15);
    case 'u': release(16);
    case 'i': release(17);
    case 'o': release(18);
    case 'p': release(19);
    case 'a': release(20);
    case 's': release(21);
    case 'd': release(22);
    case 'f': release(23);
    case 'g': release(24);
    case 'h': release(25);
    case 'j': release(26);
    case 'k': release(27);
    case 'l': release(28);
    case ';': release(29);
    case 'z': release(30);
    case 'x': release(31);
    case 'c': release(32);
    case 'v': release(33);
    case 'b': release(34);
    case 'n': release(35);
    case 'm': release(36);
    case ',': release(37);
    case '.': release(38);
    case '/': release(39);
    default: break;
    }
}

void AddSynthApp::multiplyPartials(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        float expFactor = pow(factor, i);
        mFrequencyFactors[i].set(mFrequencyFactors[i].get() * expFactor);
    }
}

void AddSynthApp::randomizePartials(float max, bool sortPartials)
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

void AddSynthApp::attackTimeAdd(float value)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAttackTimes[i].set(mAttackTimes[i].get() + value);
    }
}

void AddSynthApp::attackTimeSlope(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAttackTimes[i].set(mAttackTimes[i].get() * pow(factor, i));
    }
}

void AddSynthApp::decayTimeAdd(float value)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mDecayTimes[i].set(mDecayTimes[i].get() + value);
    }
}

void AddSynthApp::decayTimeSlope(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mDecayTimes[i].set(mDecayTimes[i].get() * pow(factor, i));
    }
}

void AddSynthApp::sustainAdd(float value)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mSustainLevels[i].set(mSustainLevels[i].get() + value);
    }
}

void AddSynthApp::sustainSlope(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mSustainLevels[i].set(mSustainLevels[i].get() * pow(factor, i));
    }
}

void AddSynthApp::releaseTimeAdd(float value)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mReleaseTimes[i].set(mReleaseTimes[i].get() + value);
    }
}

void AddSynthApp::releaseTimeSlope(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mReleaseTimes[i].set(mReleaseTimes[i].get() * pow(factor, i));
    }
}

void AddSynthApp::ampModFreqAdd(float value)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmpModFrequencies[i].set(mAmpModFrequencies[i].get() + value);
    }
}

void AddSynthApp::ampModFreqSlope(float factor)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        mAmpModFrequencies[i].set(mAmpModFrequencies[i].get() * pow(factor, i));
	}
}

void AddSynthApp::ampModRandomize(float max, bool sortPartials)
{
	if (max > 30.0) {
        max = 30.0;
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
        mAmpModFrequencies[i].set(randomFactors[i]);
    }
}

void AddSynthApp::trigger(int id)
{
//    std::cout << "trigger id " << id << std::endl;
    AddSynthParameters params;
    params.id = id;
    params.mLevel = mLevel.get();
    params.mFundamental = mFundamental.get();
    params.mCumulativeDelay = mCumulativeDelay.get();
    params.mCumDelayRandomness = mCumulativeDelayRandomness.get();
    params.mArcStart = mArcStart.get();
    params.mArcSpan = mArcSpan.get();
	params.mAttackCurve = mAttackCurve.get();
    params.mReleaseCurve = mReleaseCurve.get();
    params.mOutputRouting = outputRouting[(size_t) mLayer.get()];

    params.mFreqMod = mModType.get() == 1.0f;
    params.mAmpModAttack = mModAttack.get();
    params.mAmpModRelease = mModRelease.get();

    for (int i = 0; i < NUM_VOICES; i++) {
        params.mAttackTimes[i] = mAttackTimes[i].get();
        params.mDecayTimes[i] = mDecayTimes[i].get();
        params.mSustainLevels[i] = mSustainLevels[i].get();
        params.mReleaseTimes[i] = mReleaseTimes[i].get();
        params.mFrequencyFactors[i] = mFrequencyFactors[i].get();
        params.mAmplitudes[i] = mAmplitudes[i].get();

        params.mAmpModFrequencies[i] = mAmpModFrequencies[i].get();
        params.mAmpModDepth[i] = mModDepth.get();
    }
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        if (synth[i].done()) {
            synth[i].trigger(params);
            break;
        }
    }
}

void AddSynthApp::release(int id)
{
//    std::cout << "release id " << id << std::endl;
    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
        if (synth[i].id() == id) {
            synth[i].release();
        }
    }
}

int main(int argc, char *argv[] )
{
    int midiChannel = 1;
    if (argc > 1) {
        midiChannel = atoi(argv[1]);
    }
    AddSynthApp(midiChannel).start();

    return 0;
}
