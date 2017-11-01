#ifndef ADD_SYNTH_HPP
#define ADD_SYNTH_HPP

#include <algorithm>
#include <vector>

#include "allocore/io/al_AudioIOData.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "allocore/ui/al_Preset.hpp"


#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Envelope.h"

#define NUM_VOICES 24

#define SYNTH_POLYPHONY 16

using namespace al;
using namespace std;

class AddSynthNoteParameters {
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

class AddSynthNote {
public:
    AddSynthNote(){
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

    void trigger(AddSynthNoteParameters &params) {
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
		std::cout << "Note release " << mId << std::endl;
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
            mOutMap[i] = outputRouting[fmod(((arcStart + (arcSpan * i/(float) (NUM_VOICES - 1))) * numSpeakers ), outputRouting.size())];
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

class AddSynth {
public:
    AddSynth() {
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

		for (int i = 0; i < SYNTH_POLYPHONY; i++) {
	        if (synth[i].done()) {
	            synth[i].release();
	            break;
	        }
	    }

#ifdef SURROUND
        outputRouting = { {4, 3, 7, 6, 2 },
                          {4, 3, 7, 6, 2 },
                          {4, 3, 7, 6, 2 }
                        }
                          ;
#else
        outputRouting = {{0, 1}, {0,1}, {0,1}};
#endif
    }

    void generateAudio(AudioIOData &io)
    {
        for (int i = 0; i < SYNTH_POLYPHONY; i++) {
            if (!synth[i].done()) {
                synth[i].generateAudio(io);
                io.frame(0);
            }
        }
    }

	void allNotesOff() {
		for (int i = 0; i < SYNTH_POLYPHONY; i++) {
            synth[i].release();
        }
	}

	void trigger(int id)
	{
	    std::cout << "trigger id " << id << std::endl;
	    AddSynthNoteParameters params;
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
				std::cout << "Triggered voice " << i << std::endl;
	            break;
	        }
	    }
	}

	void release(int id)
	{
	    std::cout << "release id " << id << std::endl;
	    for (int i = 0; i < SYNTH_POLYPHONY; i++) {
	        if (synth[i].id() == id) {
	            synth[i].release();
	        }
	    }
	}


	void multiplyPartials(float factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        float expFactor = pow(factor, i);
	        mFrequencyFactors[i].set(mFrequencyFactors[i].get() * expFactor);
	    }
	}

	void randomizePartials(float max, bool sortPartials = true)
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

	void harmonicPartials()
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mFrequencyFactors[i].set(i + 1);
	    }
	}

	void oddPartials()
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mFrequencyFactors[i].set((i * 2) + 1);
	    }
	}

	void setAmpsToOne()
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAmplitudes[i].set(1.0);
	    }
	}

	void setAmpsOneOverN()
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAmplitudes[i].set(1.0/ (i + 1));
	    }
	}

	void ampSlopeFactor(double factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAmplitudes[i].set(mAmplitudes[i].get() * pow(factor, i));
	    }
	}

	void attackTimeAdd(float value)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAttackTimes[i].set(mAttackTimes[i].get() + value);
	    }
	}

	void attackTimeSlope(float factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAttackTimes[i].set(mAttackTimes[i].get() * pow(factor, i));
	    }
	}

	void decayTimeAdd(float value)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mDecayTimes[i].set(mDecayTimes[i].get() + value);
	    }
	}

	void decayTimeSlope(float factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mDecayTimes[i].set(mDecayTimes[i].get() * pow(factor, i));
	    }
	}

	void sustainAdd(float value)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mSustainLevels[i].set(mSustainLevels[i].get() + value);
	    }
	}

	void sustainSlope(float factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mSustainLevels[i].set(mSustainLevels[i].get() * pow(factor, i));
	    }
	}

	void releaseTimeAdd(float value)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mReleaseTimes[i].set(mReleaseTimes[i].get() + value);
	    }
	}

	void releaseTimeSlope(float factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mReleaseTimes[i].set(mReleaseTimes[i].get() * pow(factor, i));
	    }
	}

	void ampModFreqAdd(float value)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAmpModFrequencies[i].set(mAmpModFrequencies[i].get() + value);
	    }
	}

	void ampModFreqSlope(float factor)
	{
	    for (int i = 0; i < NUM_VOICES; i++) {
	        mAmpModFrequencies[i].set(mAmpModFrequencies[i].get() * pow(factor, i));
		}
	}

	void ampModRandomize(float max, bool sortPartials)
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

	// Presets
    PresetHandler mPresetHandler;

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

	vector<vector<int>> outputRouting;

private:

    AddSynthNote synth[SYNTH_POLYPHONY];

};

#endif // ADD_SYNTH_HPP
