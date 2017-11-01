
#include <vector>

#include "allocore/al_Allocore.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "allocore/ui/al_Preset.hpp"
#include "allocore/ui/al_PresetMIDI.hpp"
#include "allocore/ui/al_ParameterMIDI.hpp"
#include "allocore/io/al_MIDI.hpp"
#include "alloaudio/al_SoundfileBuffered.hpp"

#include "common.hpp"

#include "chaos_synth.hpp"
#include "add_synth.hpp"
#include "granulator.hpp"
#include "downmixer.hpp"

#define CHAOS_SYNTH_POLYPHONY 1
#define ADD_SYNTH_POLYPHONY 1

using namespace std;
using namespace al;


class BaseAudioApp : public AudioCallback{
public:

	BaseAudioApp(){
		mAudioIO.append(*this);
		initAudio(44100);
	}

	void initAudio(
		double framesPerSec, unsigned framesPerBuffer=128,
		unsigned outChans=2, unsigned inChans=0
	){
		mAudioIO.framesPerSecond(framesPerSec);
		mAudioIO.framesPerBuffer(framesPerBuffer);
		mAudioIO.channelsOut(outChans);
		mAudioIO.channelsIn(inChans);
		gam::sampleRate(framesPerSec);
	}

	AudioIO& audioIO(){ return mAudioIO; }

	void start(bool block=true){
		mAudioIO.start();
        int c;
		if(block){
            do {
                c=getchar();
                putchar (c);
                if (c == 'q') {
                    std::cout << "ip" << std::endl;
                }
                if (c == 'w') {
                    std::cout << "ip" << std::endl;
                }
              } while (c != '.');
		}
	}

private:
	AudioIO mAudioIO;
};

class AudioApp: public BaseAudioApp, public osc::PacketHandler
{
public:
    AudioApp(int midiChannel = 1) : BaseAudioApp()
    {
        for (int i = 0; i < 3; i++) {
            addSynth[i].outputRouting = {
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
                {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59}
            };
            addSynth3[i].outputRouting = {
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
                {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59}
            };
        }
        addSynth2.outputRouting = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
            {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59}
        };
        addSynthCampanas.outputRouting = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
            {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59}
        };


        mSequencer1a.setDirectory("sequences");
        mSequencer1a.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth[0].mFundamental.set(midi2cps(params[0]));
            app->addSynth[0].trigger(params[0]);
            std::cout << "Note On 1!! " << params[0]  << std::endl;
        }, this);
        mSequencer1a.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth[0].release(params[0]);
            std::cout << "Note Off 1!! " << params[0]  << std::endl;
        }, this);
        mSequencer1a.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program 1!! " << app->addSynth[0].mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);

        mSequencer1b.setDirectory("sequences");
        mSequencer1b.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth[1].mFundamental.set(midi2cps(params[0]));
            app->addSynth[1].trigger(params[0]);
            std::cout << "Note On 2!! " << params[0]  << std::endl;
        }, this);
        mSequencer1b.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth[1].release(params[0]);
            std::cout << "Note Off 2!! " << params[0]  << std::endl;
        }, this);
        mSequencer1b.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program 2!! " << app->addSynth[1].mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);

        mSequencer1c.setDirectory("sequences");
        mSequencer1c.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth[2].mFundamental.set(midi2cps(params[0]));
            app->addSynth[2].trigger(params[0]);
            std::cout << "Note On 3!! " << params[0]  << std::endl;
        }, this);
        mSequencer1c.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth[2].release(params[0]);
            std::cout << "Note Off 3!! " << params[0]  << std::endl;
        }, this);
        mSequencer1c.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program 3!! " << app->addSynth[2].mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);

		mSequencer1.playSequence("Seq 3");

        mSequencer2.setDirectory("sequences");
        mSequencer2.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth2.mFundamental.set(midi2cps(params[0]));
            app->addSynth2.trigger(params[0]);
            std::cout << "Note On!! " << params[0]  << std::endl;
        }, this);
        mSequencer2.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth2.release(params[0]);
            std::cout << "Note Off!! " << params[0]  << std::endl;
        }, this);
        mSequencer2.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program!! " << app->addSynth2.mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);


        mSequencer3a.setDirectory("sequences");
        mSequencer3a.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth3[0].mFundamental.set(midi2cps(params[0]));
            app->addSynth3[0].trigger(params[0]);
            std::cout << "Note On!! " << params[0]  << std::endl;
        }, this);
        mSequencer3a.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth3[0].release(params[0]);
            std::cout << "Note Off!! " << params[0]  << std::endl;
        }, this);
        mSequencer3a.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program!! " << app->addSynth3[0].mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);

        mSequencer3b.setDirectory("sequences");
        mSequencer3b.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth3[1].mFundamental.set(midi2cps(params[0]));
            app->addSynth3[1].trigger(params[0]);
            std::cout << "Note On!! " << params[0]  << std::endl;
        }, this);
        mSequencer3b.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth3[1].release(params[0]);
            std::cout << "Note Off!! " << params[0]  << std::endl;
        }, this);
        mSequencer3b.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program!! " << app->addSynth3[1].mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);

        mSequencer3c.setDirectory("sequences");
        mSequencer3c.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth3[2].mFundamental.set(midi2cps(params[0]));
            app->addSynth3[2].trigger(params[0]);
            std::cout << "Note On!! " << params[0]  << std::endl;
        }, this);
        mSequencer3c.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth3[2].release(params[0]);
            std::cout << "Note Off!! " << params[0]  << std::endl;
        }, this);
        mSequencer3c.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program!! " << app->addSynth3[2].mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);
    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    void init() {
        mFromSimulator.handler(*this);
        mFromSimulator.timeout(0.005);
        mFromSimulator.start();
    }

    virtual void onAudioCB(AudioIOData &io) override;
    virtual void onMessage(osc::Message &m) override {
        if (m.addressPattern() == "/chaos" && m.typeTags() == "f") {
            mPrevChaos = mChaos;
            m >> mChaos;
        }
    }

private:
    // Synthesis
    AddSynth addSynth[3];
    AddSynth addSynth2;
    AddSynth addSynth3[3];
    AddSynth addSynthCampanas;

    // Sequence players
    PresetSequencer mSequencer1a;
    PresetSequencer mSequencer1b;
    PresetSequencer mSequencer1c;
    PresetSequencer mSequencer2;
    PresetSequencer mSequencer3a;
    PresetSequencer mSequencer3b;
    PresetSequencer mSequencer3c;
    PresetSequencer mSequencer4a;
    PresetSequencer mSequencer4b;
    PresetSequencer mSequencer4c;

    // Schedule Messages
    MsgQueue msgQueue;

    DownMixer mDownMixer;

    osc::Recv mFromSimulator {AUDIO_IN_PORT, AUDIO_IP_ADDRESS};

    float mChaos {0};
    float mPrevChaos {0};
};

static void releaseAddSynth(al_sec timestamp, AddSynth *addSynth, int id)
{
    addSynth->release(id);
    std::cout << "release" << std::endl;
}

void AudioApp::onAudioCB(AudioIOData &io)
{
    int bufferSize = io.framesPerBuffer();
    float *swBuffer = io.outBuffer(47);

    // Campanitas
	float max = 0.3;
    if (mChaos < max) {
        float probCampanitas = 0.01 + (mChaos/max) * 0.006;
        if (rnd::prob(probCampanitas)) {
            std::cout << "trigger" << std::endl;
            addSynthCampanas.mPresetHandler.recallPresetSynchronous("34");
            addSynthCampanas.mLayer = rnd::uniform(3);
            addSynthCampanas.mArcSpan = rnd::uniform(0.5, 2.0);
            addSynthCampanas.mArcStart = rnd::uniform();
            addSynthCampanas.mCumulativeDelayRandomness = addSynthCampanas.mCumulativeDelayRandomness +rnd::uniform(0.2, -0.2);
            addSynthCampanas.mFundamental = addSynthCampanas.mFundamental + rnd::uniform(40, -40);
            addSynthCampanas.trigger(0);
            msgQueue.send(msgQueue.now() + 2.5, releaseAddSynth, &addSynthCampanas, 0);
        }
    }

    for (int i = 0; i < 3; i++) {
        addSynth[i].generateAudio(io);
        addSynth3[i].generateAudio(io);
    }
    addSynth2.generateAudio(io);
    addSynthCampanas.generateAudio(io);

    /// Sequences
    ///
    ///
    float rangeStart = 0.1;
    float rangeEnd = 0.3;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeStart &&  mChaos <= rangeStart)
            ) {
        if (!mSequencer1a.running()) {
            addSynth2.allNotesOff();
            mSequencer1a.playSequence("Seq 1-1");
            mSequencer1b.playSequence("Seq 1-2");
            mSequencer1c.playSequence("Seq 1-3");
            std::cout << "Seq 1" << std::endl;
        }
    }
    rangeStart = 0.28;
    rangeEnd = 0.5;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeStart &&  mChaos <= rangeStart)
            ) {
        if (!mSequencer2.running()) {
            addSynth[0].allNotesOff();
            addSynth[1].allNotesOff();
            addSynth[2].allNotesOff();
            addSynth3[0].allNotesOff();
            addSynth3[1].allNotesOff();
            addSynth3[2].allNotesOff();
            mSequencer2.playSequence("Seq 2-1");
            std::cout << "Seq 2" << std::endl;
        }
    }

    rangeStart = 0.2;
    rangeEnd = 0.6;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeStart &&  mChaos <= rangeStart)
            ) {
        if (!mSequencer3a.running()) {
            addSynth2.allNotesOff();
            mSequencer3a.playSequence("Seq 3-1");
            mSequencer3b.playSequence("Seq 3-2");
            mSequencer3c.playSequence("Seq 3-2");
            std::cout << "Seq 3" << std::endl;
        }
    }

    rangeStart = 0.6;
    rangeEnd = 0.8;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeEnd &&  mChaos <= rangeEnd)
            ) {
//        addSynth3.allNotesOff();
        mSequencer4a.playSequence("Seq 4a-0");
        mSequencer4b.playSequence("Seq 4b-0");
        mSequencer4c.playSequence("Seq 4c-0");
        std::cout << "Seq 3" << std::endl;
    }

    for (int i = 0; i < bufferSize; i++) {
        *swBuffer++ *= 0.07;
    }

    msgQueue.advance(io.framesPerBuffer()/io.framesPerSecond());

//    std::cout << "-------------" << std::endl;
//    for (int i= 0; i < 60; i++) {
//        std::cout << io.out(i, 0) << " ";
//    }
//    std::cout << std::endl;

//    mDownMixer.process(io);
}

int main(int argc, char *argv[] )
{
    AudioApp app;

    int outChans = 60;
#ifdef BUILDING_FOR_ALLOSPHERE
    app.audioIO().device(AudioDevice("ECHO X5"));
#endif
    app.initAudio(44100, 512, outChans, 0);
    gam::sampleRate(app.audioIO().fps());

//    AudioDevice::printAll();
    app.audioIO().print();
    app.init();
    app.start();
    return 0;
}
