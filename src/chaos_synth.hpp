#ifndef CHAOS_SYNTH_HPP
#define CHAOS_SYNTH_HPP

#include <vector>

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Envelope.h"
#include "Gamma/Analysis.h"

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
		mEnv.release();
    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

	void setOutputIndeces(int index1, int index2) {
		mOutputChannels[0] = index1;
		mOutputChannels[1] = index2;
	}

	bool done() {
		return mEnv.done();
	}

    void trigger(int id) {
		mId = id;
//        mLevel = params.mLevel;
        mEnv.resetSoft();

		int bassChannel = mBassChannel + rnd::uniform(-8, 8);
		bassChannel %= 60;
		if (bassChannel < 0) {
			bassChannel += 60;
		}
		while (mNoiseChannel == 47
		       || mNoiseChannel == 12 || mNoiseChannel == 13 || mNoiseChannel == 14 || mNoiseChannel == 15
		       || mNoiseChannel == 46
		       ) {
			bassChannel += rnd::uniform(-10, 10);
			bassChannel %= 60;
			if (bassChannel < 0) {
				bassChannel += 60;
			}
		}
		mBassChannel = bassChannel;

		int noiseChannel = mNoiseChannel + rnd::uniform(-8, 8);
		noiseChannel %= 60;
		if (noiseChannel < 0) {
			noiseChannel += 60;
		}
		while (mNoiseChannel == 47
		       || mNoiseChannel == 12 || mNoiseChannel == 13 || mNoiseChannel == 14 || mNoiseChannel == 15
		       || mNoiseChannel == 46
		       ) {
			noiseChannel += rnd::uniform(-8, 8);
			noiseChannel %= 60;
			if (noiseChannel < 0) {
				noiseChannel += 60;
			}
		}
		mNoiseChannel = noiseChannel;
    }

    void release(int id) {
        mEnv.release();
    }

	float mTrim = 1.0;

    // Presets
    PresetHandler mPresetHandler {"chaosPresets"};

    Parameter mLevel {"Level", "", 0.5, "", 0, 1.0};

    // basstone |
    Parameter phsrFreq1 {"phsrFreq1", "", 440, "", 0.0001, 9999.0};
    Parameter phsrFreq2 {"phsrFreq2", "", 440, "", 0.0001, 9999.0};
    Parameter bassFilterFreq {"bassFilterFreq", "", 0, "", 0.0, 9999.0};

    gam::Saw<> mOsc1, mOsc2;
    gam::Biquad<> mOscBandPass { 1000, 5, gam::BAND_PASS};
    gam::Biquad<> mOscLowPass {100, 5, gam::LOW_PASS};
	gam::SilenceDetect mSilenceDetect;
	int mBassChannel {0};

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
	int mNoiseChannel {0};

    // Randomness
    Parameter changeProb {"changeProb", "", 0.02, "", 0, 1};
    Parameter changeDev {"changeDev", "", 0.1, "", 0, 99.0};
    float freqDev1 = 0 , freqDev2 = 0;

    //
    al::Reverb<> mReverb;
	al::Reverb<> mReverbNoise;

    gam::BlockDC<> mDCBlockL, mDCBlockR;
	gam::BlockDC<> mDCBlockNoise;
    gam::ADSR<> mEnv{3.0, 0.0, 1.0, 3.0};

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
			float outerenv = mEnv();
            float basstone = (env * 0.5) * (mOsc1() + mOsc2()) * outerenv;

			float revOutL, revOutR;
			if(mSilenceDetect(basstone)) {
				mBassChannel += rnd::uniform(-8, 8);
				mBassChannel %= 60;
				if (mBassChannel < 0) {
					mBassChannel += 60;
				}
				while (mNoiseChannel == 47
				       || mNoiseChannel == 12 || mNoiseChannel == 13 || mNoiseChannel == 14 || mNoiseChannel == 15
				       || mNoiseChannel == 46
				       ) {
					mBassChannel += rnd::uniform(-8, 8);
					mBassChannel %= 60;
					if (mBassChannel < 0) {
						mBassChannel += 60;
					}
				}
			}
            mReverb(basstone, revOutL, revOutR);
			outL = mDCBlockL(revOutL);
            outR = mDCBlockR(revOutR);
			outL = outL * mLevel * 0.05;
			outR = outR * mLevel * 0.05;
			if (mOsc1.freq() > 0.001 && mOsc2.freq() > 0.001) {
//				io.out(mOutputChannels[0]) = outL * 0.8;
//				io.out(mOutputChannels[1]) = outR * 0.8;
//				io.out(mOutputChannels[2]) = outL * 0.3;
//				io.out(mOutputChannels[3]) = outR * 0.3;
				io.out(mBassChannel) =  basstone * 0.2* mTrim;
				io.out(47) +=  (basstone /*+ outL + outR*/) * 0.5* mTrim;
			}

            // noise |
            float noiseOut;
            if (mTrigger()) {
                noiseHold = mNoise();
            }

            if (rnd::prob(0.0001)) {
                mNoiseEnv.reset();
				mNoiseChannel += rnd::uniform(-8, 8);
				mNoiseChannel %= 60;
				if (mNoiseChannel < 0) {
					mNoiseChannel += 60;
				}
				while (mNoiseChannel == 47
				       || mNoiseChannel == 12 || mNoiseChannel == 13 || mNoiseChannel == 14 || mNoiseChannel == 15
				       || mNoiseChannel == 46
				       ) {
					mNoiseChannel += rnd::uniform(-8, 8);
					mNoiseChannel %= 60;
					if (mNoiseChannel < 0) {
						mNoiseChannel += 60;
					}
				}
            }
            noiseOut = mDCBlockNoise(noiseHold* outerenv) * mNoiseEnv() * 0.05 * mTrim;

            // output

			io.out(mNoiseChannel) += noiseOut;
			mReverbNoise(noiseOut, revOutL, revOutR);

//			io.out(20) += revOutL * 0.1;
//			io.out(26) += revOutR * 0.1;
			io.out(47) += (/*revOutR + revOutL +*/ noiseOut) * 0.07 * mTrim;

        }
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
			if (value != 0) {
				static_cast<ChaosSynth *>(userData)->mOsc1.freq(value);
			}
//            std::cout << "new freq " << value << std::endl;
        }, this);
        phsrFreq2.registerChangeCallback([] (float value, void *sender,
                                         void *userData, void * blockSender){
			if (value != 0) {
				static_cast<ChaosSynth *>(userData)->mOsc2.freq(value);
			}
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

    vector<int> mOutputChannels {18, 28, 21, 26};

    int mId = 0;
};



#endif // CHAOS_SYNTH_HPP
