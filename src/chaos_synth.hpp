#ifndef CHAOS_SYNTH_HPP
#define CHAOS_SYNTH_HPP

#include <vector>

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Envelope.h"

#include "allocore/ui/al_Parameter.hpp"
#include "allocore/ui/al_Preset.hpp"
#include "allocore/sound/al_Reverb.hpp"
#include "allocore/math/al_Random.hpp"
#include "allocore/io/al_AudioIOData.hpp"

using namespace al;
using namespace std;

class ChaosSynthParameters {
public:
    int id; // Instance id (e.g. MIDI note)
    float mLevel;
//    vector<int> mOutputRouting;
};

class ChaosSynth {
public:
    ChaosSynth(){
        mPresetHandler << mLevel; // << mFundamental;
        mPresetHandler << envFreq1 << envFreq2;
        mPresetHandler << phsrFreq1 << phsrFreq2;
        mPresetHandler << noiseRnd;
        mPresetHandler << changeProb << changeDev;

        connectCallbacks();

//        mEnv.sustainPoint(1);

        resetClean();
    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    void trigger(ChaosSynthParameters &params) {
//        mLevel = params.mLevel;
        mEnv.reset();
    }

    void release() {
        mEnv.release();
    }

    // Presets
    PresetHandler mPresetHandler {"chaosPresets"};

    Parameter mLevel {"Level", "", 0.5, "", 0, 1.0};

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
    al::Reverb<> mReverb;

    gam::BlockDC<> mDCBlockL, mDCBlockR;
    gam::ADSR<> mEnv{0.5, 0.0, 1.0, 3.0};

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
            io.out(mOutputChannels[0]) = outL * mLevel * 0.3 * outerenv;
            io.out(mOutputChannels[1]) = outR * mLevel * 0.3 * outerenv;

        }
    }


    bool done() {
        return mDone;
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

private:
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

    vector<int> mOutputChannels {0, 1};

    int mId = 0;
    bool mDone {false};
};



#endif // CHAOS_SYNTH_HPP
