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

    // Spatialization
    float mArcStart;
    float mArcSpan;
    vector<int> mOutputRouting;
};

class AddSynth {
public:
    AddSynth(){
        float envLevels[6] = {0.0, 0.0, 1.0, 0.7, 0.7, 0.0};
        float ampModEnvLevels[4] = {0.0, 1.0, 1.0, 0.0};
        for (int i = 0; i < NUM_VOICES; i++) {
            mEnvelopes[i].levels(envLevels, 6).sustainPoint(4).finish();
            mAmpModEnvelopes[i].levels(ampModEnvLevels, 4).sustainPoint(3).finish();
        }
        setCurvature(-4);
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

        for (int i = 0; i < NUM_VOICES; i++) {
            setAttackTime(params.mAttackTimes[i], i);
            setDecayTime(params.mDecayTimes[i], i);
            setSustainLevel(params.mSustainLevels[i], i);
            setReleaseTime(params.mReleaseTimes[i], i);
            setAmpModAttackTime(params.mAmpModAttack, i);
            setAmpModReleaseTime(params.mAmpModRelease, i);
            mAmpModulators[i].set(params.mAmpModFrequencies[i], params.mAmpModDepth[i]);

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
                io.out(mOutMap[i]) += mOscillators[i]() * mEnvelopes[i]() *  mAmplitudes[i] * mLevel
                        * (1 + mAmpModulators[i]() * mAmpModEnvelopes[i]());
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
        mAmpModEnvelopes[i].lengths()[2] = releaseTime;
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

    int mId = 0;
    float mLevel = 0;
    float mFrequencyFactors[NUM_VOICES];
    float mAmplitudes[NUM_VOICES];

    int mOutMap[NUM_VOICES] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

};

class PresetKeyboardControl : public NavInputControl {
public:

	PresetKeyboardControl (Nav &nav) : NavInputControl(nav){ mUseMouse = false;}

	virtual bool onKeyDown(const Keyboard &k) {
		if (k.ctrl() || k.alt() || k.meta()) {
			return true;
		}
//		if (!presetKeyboardActive) {
//			return true;
//		}
		switch(k.key()){
		case '1': presets->recallPreset(0); return false;
		case '2': presets->recallPreset(1); return false;
		case '3': presets->recallPreset(2); return false;
		case '4': presets->recallPreset(3); return false;
		case '5': presets->recallPreset(4); return false;
		case '6': presets->recallPreset(5); return false;
		case '7': presets->recallPreset(6); return false;
		case '8': presets->recallPreset(7); return false;
		case '9': presets->recallPreset(8); return false;
		case '0': presets->recallPreset(9); return false;
		case 'q': presets->recallPreset(10); return false;
		case 'w': presets->recallPreset(11); return false;
		case 'e': presets->recallPreset(12); return false;
		case 'r': presets->recallPreset(13); return false;
		case 't': presets->recallPreset(14); return false;
		case 'y': presets->recallPreset(15); return false;
		case 'u': presets->recallPreset(16); return false;
		case 'i': presets->recallPreset(17); return false;
		case 'o': presets->recallPreset(18); return false;
		case 'p': presets->recallPreset(19); return false;
		case 'a': presets->recallPreset(20); return false;
		case 's': presets->recallPreset(21); return false;
		case 'd': presets->recallPreset(22); return false;
		case 'f': presets->recallPreset(23); return false;
		case 'g': presets->recallPreset(24); return false;
		case 'h': presets->recallPreset(25); return false;
		case 'j': presets->recallPreset(26); return false;
		case 'k': presets->recallPreset(27); return false;
		case 'l': presets->recallPreset(28); return false;
		case ';': presets->recallPreset(29); return false;
		case 'z': presets->recallPreset(30); return false;
		case 'x': presets->recallPreset(31); return false;
		case 'c': presets->recallPreset(32); return false;
		case 'v': presets->recallPreset(33); return false;
		case 'b': presets->recallPreset(34); return false;
		case 'n': presets->recallPreset(35); return false;
		case 'm': presets->recallPreset(36); return false;
		case ',': presets->recallPreset(37); return false;
		case '.': presets->recallPreset(38); return false;
		case '/': presets->recallPreset(39); return false;
		default: break;
		}
		return true;
	}

	virtual bool onKeyUp(const Keyboard &k) {
		return true;
	}
    PresetHandler *presets;
};

class AddSynthApp: public App
{
public:
    AddSynthApp() : mKeyboardPresets(nav())
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

        windows()[0]->prepend(mKeyboardPresets);
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

    // Envelope
    void trigger(int id);
    void release(int id);
    void setInitialCumulativeDelay(float initialDelay);
    void setAttackTime(float attackTime);
    void setReleaseTime(float releaseTime);
    void setCurvature(float curvature);

private:

    // Parameters
    Parameter mLevel {"Level", "", 0.05, "", 0.0, 1.0};
    Parameter mFundamental {"Fundamental", "", 220.0, "", 0.0, 9000.0};
    Parameter mCumulativeDelay{"CumDelay", "", 0.0, "", -0.2, 0.2};
    Parameter mCumulativeDelayRandomness{"CumDelayDev", "", 0.0, "", 0.0, 1.0};

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
        {"Attack16", "", 0.1f, "", 0.0, 20.0},
        {"Attack17", "", 0.1f, "", 0.0, 20.0},
        {"Attack18", "", 0.1f, "", 0.0, 20.0},
        {"Attack19", "", 0.1f, "", 0.0, 20.0},
        {"Attack20", "", 0.1f, "", 0.0, 20.0},
        {"Attack21", "", 0.1f, "", 0.0, 20.0},
        {"Attack22", "", 0.1f, "", 0.0, 20.0},
        {"Attack23", "", 0.1f, "", 0.0, 20.0},
        {"Attack24", "", 0.1f, "", 0.0, 20.0}
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
        {"Harm16", "", 16.0f, "", 0.0, 20.0},
        {"Harm17", "", 9.0f, "", 0.0, 20.0},
        {"Harm18", "", 10.0f, "", 0.0, 20.0},
        {"Harm19", "", 11.0f, "", 0.0, 20.0},
        {"Harm20", "", 12.0f, "", 0.0, 20.0},
        {"Harm21", "", 13.0f, "", 0.0, 20.0},
        {"Harm22", "", 14.0f, "", 0.0, 20.0},
        {"Harm23", "", 15.0f, "", 0.0, 20.0},
        {"Harm24", "", 16.0f, "", 0.0, 20.0}
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
    Parameter mAmpModDepth{"AmpModDepth", "", 0.0, "", 0.0, 1.0};
    Parameter mAmpModAttack{"AmpModAttack", "", 0.0, "", 0.0, 10.0};
    Parameter mAmpModRelease{"AmpModRelease", "", 0.0, "", 0.0, 10.0};

    vector<Parameter> mAmpModFrequencies = {
        {"AmpModFreq1", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq2", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq3", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq4", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq5", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq6", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq7", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq8", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq9", "", 1.0f, "", 0.0, 30.0},
        {"AmpModFreq10", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq11", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq12", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq13", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq14", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq15", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq16", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq17", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq18", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq19", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq20", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq21", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq22", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq23", "",1.0f, "", 0.0, 30.0},
        {"AmpModFreq24", "",1.0f, "", 0.0, 30.0}
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
    GLVDetachable controlView;

    PresetKeyboardControl mKeyboardPresets;

    // MIDI Control
    PresetMIDI presetMIDI;
    ParameterMIDI parameterMIDI;

    MIDIIn midiIn {"USB Oxygen 49"};

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    static void midiCallback(double deltaTime, std::vector<unsigned char> *msg, void *userData){
        AddSynthApp *app = static_cast<AddSynthApp *>(userData);
        unsigned numBytes = msg->size();

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
                default:;
                }
            }
        }
    }

    // Synthesis
    AddSynth synth[SYNTH_POLYPHONY];
    vector<int> outputRouting;
};

void AddSynthApp::initializeValues()
{
    mFundamental.set(220);
    harmonicPartials();

    mLevel.set(0.05);
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

    ampModBox << ParameterGUI::makeParameterView(mAmpModDepth);
    ampModBox << ParameterGUI::makeParameterView(mAmpModAttack);
    ampModBox << ParameterGUI::makeParameterView(mAmpModRelease);

    for (int i = 0; i < NUM_VOICES; i++) {
        ampModBox << ParameterGUI::makeParameterView(mAmpModFrequencies[i]);
    }
    ampModBox.fit();
    ampModBox.enable(glv::DrawBack);
    ampModBox.anchor(glv::Place::TR);
    ampModBox.set(-400, 30, releaseTimesBox.width(), releaseTimesBox.height());
    controlView << ampModBox;


    controlView.set(300, 0, 300, 400);
}

void AddSynthApp::initializePresets()
{
    mPresetHandler << mLevel;
    mPresetHandler << mFundamental << mCumulativeDelay << mCumulativeDelayRandomness;
    mPresetHandler << mArcStart << mArcSpan;
    for (int i = 0; i < NUM_VOICES; i++) {
        mPresetHandler << mFrequencyFactors[i] << mAmplitudes[i];
        mPresetHandler << mAttackTimes[i] << mDecayTimes[i] << mSustainLevels[i] << mReleaseTimes[i];
    }
    mPresetHandler.print();
    sequencer << mPresetHandler;
    recorder << mPresetHandler;
    mKeyboardPresets.presets = &mPresetHandler;

    // MIDI Control of parameters
    unsigned int midiControllerPort = 4;
    parameterMIDI.init(midiControllerPort);
    parameterMIDI.connectControl(mCumulativeDelay, 75, 1);
    parameterMIDI.connectControl(mCumulativeDelayRandomness, 76, 1);
    parameterMIDI.connectControl(mArcStart, 77, 1);
    parameterMIDI.connectControl(mArcSpan, 78, 1);

    // MIDI control of presets
    // 74 71 91 93 73 72 5 84 7
    // 75 76 77 78 74 71 24 102
    presetMIDI.init(midiControllerPort, mPresetHandler);
    presetMIDI.setMorphControl(102, 1, 0.0, 8.0);
    // MIDI preset mapping
//    presetMIDI.connectNoteToPreset(1, 0, 36, 24, 59);
    unsigned portToOpen = 4;
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
    trigger(k.key());
}

void AddSynthApp::onKeyUp(const Keyboard &k)
{
    release(k.key());
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
    params.mOutputRouting = outputRouting;

    params.mAmpModAttack = mAmpModAttack.get();
    params.mAmpModRelease = mAmpModRelease.get();

    for (int i = 0; i < NUM_VOICES; i++) {
        params.mAttackTimes[i] = mAttackTimes[i].get();
        params.mDecayTimes[i] = mDecayTimes[i].get();
        params.mSustainLevels[i] = mSustainLevels[i].get();
        params.mReleaseTimes[i] = mReleaseTimes[i].get();
        params.mFrequencyFactors[i] = mFrequencyFactors[i].get();
        params.mAmplitudes[i] = mAmplitudes[i].get();

        params.mAmpModDepth[i] = mAmpModDepth.get();
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
    AddSynthApp().start();

    return 0;
}
