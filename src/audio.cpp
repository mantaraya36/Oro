
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

class AudioApp: public App, public osc::PacketHandler
{
public:
    AudioApp(int midiChannel = 1) : App()
    {
        addSynth.outputRouting = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
            {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59}
        };
        for (auto baseNames : mCamasFilenames) {
            mCamaFiles.push_back(std::vector<std::shared_ptr<SoundFileBuffered>>());
            for (auto components: mComponentMap) {
                std::string filename = "Texturas base/Camas/" + baseNames + components + ".wav";
                mCamaFiles.back().push_back(std::make_shared<SoundFileBuffered>(filename, true, 8192));
                std::cout << filename << "  " << mCamaFiles.back().back()->channels() << std::endl;
            }
        }
//        mBaseOn[3] = true;

        std::vector<std::string> VoicesFilenames = {
            "Cura 7Ch/Cura 7Ch.C.wav",   "Cura 7Ch/Cura 7Ch.L.wav",   "Cura 7Ch/Cura 7Ch.R.wav",
            "Cura 7Ch/Cura 7Ch.Lc.wav",  "Cura 7Ch/Cura 7Ch.Rc.wav",
            "Cura 7Ch/Cura 7Ch.Ls.wav",  "Cura 7Ch/Cura 7Ch.Rs.wav"
        };

        for (auto filename: VoicesFilenames) {
            mVoices.push_back(std::make_shared<SoundFileBuffered>("Texturas base/Atras/" + filename, true, 8192));
            if (!mVoices.back()->opened()) {
                std::cout << "Can't find " << filename << std::endl;
            }
        }

        mSequencer1.setDirectory("sequences");
        mSequencer1.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth.trigger(params[0]);
            std::cout << "Note On!! " << params[0]  << std::endl;
        }, this);
        mSequencer1.registerEventCommand("OFF", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            app->addSynth.release(params[0]);
            std::cout << "Note Off!! " << params[0]  << std::endl;
        }, this);
        mSequencer1.registerEventCommand("PROGRAM",
                                         [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
            std::cout << "Program!! " << app->addSynth.mPresetHandler.recallPresetSynchronous(params[0]) << std::endl;
        }, this);


        mSequencer2.setDirectory("sequences");
        mSequencer2.registerEventCommand("ON", [](void *data, std::vector<float> &params)
        {
            AudioApp *app = static_cast<AudioApp *>(data);
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
    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    void init() {
        mFromSimulator.handler(*this);
        mFromSimulator.timeout(0.005);
        mFromSimulator.start();
        mVocesEnv.sustainPoint(1);
        mVocesEnv.lengths()[1] = 1.2;
        mVocesEnv.lengths()[2] = 1.2;
        mVocesEnv.release();
    }

    virtual void onSound(AudioIOData &io) override;
    virtual void onMessage(osc::Message &m) override {
        if (m.addressPattern() == "/chaos" && m.typeTags() == "f") {
            mPrevChaos = mChaos;
            m >> mChaos;
        } else if (m.addressPattern() == "/mouseDown" && m.typeTags() == "f") {
            float val;
            m >> val;
            if (val == 0) {
                mVocesEnv.release();
            } else {
                mVocesEnv.resetSoft();
            }
            m.print();
        }
    }

private:
    // Synthesis
    ChaosSynth chaosSynth[CHAOS_SYNTH_POLYPHONY];
    AddSynth addSynth;
    AddSynth addSynth2;

/// Camas
    std::vector<int> mCamasRouting = {49, 58, 52, 55,23,26, 30, 34 , 38, 42, 16 , 20 , 1, 10, 4, 7 };

    std::vector<std::string> mComponentMap {
        "Lower Circle.L" ,
        "Lower Circle.Ls",
        "Lower Circle.R" ,
        "Lower Circle.Rs",
        "Mid Circle.C" ,
        "Mid Circle.L" ,
        "Mid Circle.Lsr" ,
        "Mid Circle.Lss" ,
        "Mid Circle.R" ,
        "Mid Circle.Rctr",
        "Mid Circle.Rsr" ,
        "Mid Circle.Rss" ,
        "Upper Circle.L",
        "Upper Circle.Ls",
        "Upper Circle.R",
        "Upper Circle.Rs"};

    std::vector<std::string> mCamasFilenames {
        "Cama01_16Ch_",
        "Cama02_Hydro_16Ch_2_",
        "Cama02_Hydro_16Ch_",
        "Cama03a_Hydro_16Ch_",
        "Cama03_Hydro_16Ch_"
    };

    float readBuffer[8192];

    std::vector<std::vector<std::shared_ptr<SoundFileBuffered>>> mCamaFiles;

    // Voices

//    "Cura 7Ch/Cura 7Ch.C.wav",   "Cura 7Ch/Cura 7Ch.L.wav",   "Cura 7Ch/Cura 7Ch.R.wav",
//    "Cura 7Ch/Cura 7Ch.Lc.wav",  "Cura 7Ch/Cura 7Ch.Rc.wav",
//    "Cura 7Ch/Cura 7Ch.Ls.wav",  "Cura 7Ch/Cura 7Ch.Rs.wav"
    std::vector<int> mVoicesRouting = {38, 40, 36, 10, 7, 42, 34 };
    std::vector<std::shared_ptr<SoundFileBuffered>> mVoices;
    gam::ADSR<> mVocesEnv {0.3, 0.3, 0.9, 2.0};

    // Sequence players
    PresetSequencer mSequencer1;
    PresetSequencer mSequencer2;

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

static void releaseChaosSynth(al_sec timestamp, ChaosSynth *chaosSynth, int id)
{
    chaosSynth->release(id);
    std::cout << "release chaos" << std::endl;
}

void readFile(std::vector<std::shared_ptr<SoundFileBuffered>> files,
              float *readBuffer,
              AudioIOData &io,
              std::vector<int> routing,
              float gain = 1.0) {
    int bufferSize = io.framesPerBuffer();
    float *swBuffer = io.outBuffer(47);

    int counter = 0;
    for (auto f : files) {
        assert(bufferSize < 8192);
        if (f->read(readBuffer, bufferSize) == bufferSize) {
            float *buf = readBuffer;
            float *bufsw = swBuffer;
            float *outbuf = io.outBuffer(routing[counter]);
            while (io()) {
                float out = *buf++ * gain;
                *outbuf++ += out;
                *bufsw++ += out;
            }
            io.frame(0);
        } else {
            //                    std::cout << "Error" << std::endl;
        }
        counter++;
    }
}

void AudioApp::onSound(AudioIOData &io)
{
    int bufferSize = io.framesPerBuffer();
    float *swBuffer = io.outBuffer(47);

    ///// Bases ---------

    std::vector<float> mCamasGains = {0.1, 0.1, 0.1, 0.1, 0.1};

    int fileIndex = 0;
    if (mChaos < 0.2) {
        fileIndex = 0;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex]);
    } else if (mChaos < 0.3) {
        float gainIndex = (mChaos - 0.2) * 10;

        fileIndex = 0;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 1;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] *gainIndex);

    }  else if (mChaos < 0.4) {
        fileIndex = 1;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex]);

    } else if (mChaos < 0.5) {
        float gainIndex = (mChaos - 0.4) * 10;

        fileIndex = 1;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 2;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] *gainIndex);

    } else if (mChaos < 0.6) {

        fileIndex = 2;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex]);

    } else if (mChaos < 0.7) {

        float gainIndex = (mChaos - 0.6) * 10;

        fileIndex = 2;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 3;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] *gainIndex);

    } else if (mChaos < 0.8) {

        fileIndex = 3;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex]);


    }  else if (mChaos < 0.9) {

        float gainIndex = (mChaos - 0.8) * 10;

        fileIndex = 3;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 4;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex] *gainIndex);

    } else {
        fileIndex = 4;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mCamasRouting, mCamasGains[fileIndex]);

    }

    // Voces atras
    float vocesGain = 0.0;
    const float vocesGainTarget = 0.4;

    if (mChaos > 0.4) {
        vocesGain = vocesGainTarget * ((mChaos - 0.4)/ 0.45);
    } else if (mChaos > 0.85) {
        vocesGain = vocesGainTarget;
    }
    for (int i = 0; i < mVoices.size(); i++) {
        assert(bufferSize < 8192);
        if (mVoices[i]->read(readBuffer, bufferSize) == bufferSize) {
            float *buf = readBuffer;
            float *bufsw = swBuffer;
            float *outbuf = io.outBuffer(mVoicesRouting[i]);
            while (io()) {
                float out = *buf++ * vocesGain * mVocesEnv();
                *outbuf++ += out;
                *bufsw++ += out;
            }
            io.frame(0);
        } else {
            //                    std::cout << "Error" << std::endl;
        }
    }

    msgQueue.advance(io.framesPerBuffer()/io.framesPerSecond());

    // Campanitas
    if (mChaos < 0.3) {
//        float probCampanitas = 0.001 + (mChaos/0.3) * 0.006;
//        if (rnd::prob(probCampanitas)) {
//            std::cout << "trigger" << std::endl;
//            addSynth.mPresetHandler.recallPresetSynchronous("34");
//            addSynth.mLayer = rnd::uniform(3);
//            addSynth.mArcSpan = rnd::uniform(0.5, 2.0);
//            addSynth.mArcStart = rnd::uniform();
//            addSynth.mCumulativeDelayRandomness = addSynth.mCumulativeDelayRandomness +rnd::uniform(0.2, -0.2);
//            addSynth.mFundamental = addSynth.mFundamental + rnd::uniform(40, -40);
//            addSynth.trigger(0);
//            msgQueue.send(msgQueue.now() + 2.5, releaseAddSynth, &addSynth, 0);
//        }
    }

    ////// Rangos de chaos para chaos synth
    float rangeStart, rangeEnd;
    rangeStart = 0.1;
    rangeEnd = 0.2;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeEnd &&  mChaos <= rangeEnd)
            ) {
        chaosSynth[0].mPresetHandler.recallPreset("12");
        chaosSynth[0].setOutputIndeces(rnd::uniform(47, 16),rnd::uniform(47, 16));
        chaosSynth[0].trigger(0);
    } else if (mPrevChaos > rangeEnd &&  mChaos <= rangeEnd) {
        chaosSynth[0].release(0);
    }
 ///////////////////////////
    rangeStart = 0.2;
    rangeEnd = 0.25;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeEnd &&  mChaos <= rangeEnd)
            ) {
        chaosSynth[0].mPresetHandler.recallPreset(0);
        chaosSynth[0].setOutputIndeces(rnd::uniform(47, 16),rnd::uniform(47, 16));
        chaosSynth[0].trigger(0);
    } else if (mPrevChaos > rangeStart &&  mChaos <= rangeStart) {
        chaosSynth[0].release(0);
    }
    ////////
    rangeStart = 0.25;
    rangeEnd = 0.3;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeEnd &&  mChaos <= rangeEnd)
            ) {
        chaosSynth[0].mPresetHandler.recallPreset(0);
        chaosSynth[0].setOutputIndeces(rnd::uniform(47, 16),rnd::uniform(47, 16));
        chaosSynth[0].trigger(0);
    } else if (mPrevChaos > rangeStart &&  mChaos <= rangeStart) {
        chaosSynth[0].release(0);
    }
    if (mChaos > rangeStart && mChaos < rangeEnd) {
        if (rnd::prob(0.0036)) {
            chaosSynth[0].mPresetHandler.setMorphTime(3 + rnd::uniform(1.0, -1.0));
            chaosSynth[0].mPresetHandler.recallPreset(0);
        } else if (rnd::prob(0.0035)) {
            chaosSynth[0].mPresetHandler.setMorphTime(3 + rnd::uniform(1.0, -1.0));
            chaosSynth[0].mPresetHandler.recallPreset(1);
        }
    }
    /////////////////////////
    rangeStart = 0.3;
    rangeEnd = 0.4;
    if ((mPrevChaos < rangeStart &&  mChaos >= rangeStart)
            || (mPrevChaos > rangeEnd &&  mChaos <= rangeEnd)
            ) {
        chaosSynth[0].mPresetHandler.recallPreset(0);
        chaosSynth[0].setOutputIndeces(rnd::uniform(47, 16),rnd::uniform(47, 16));
        chaosSynth[0].trigger(0);
    } else if (mPrevChaos > 0.3 &&  mChaos <= 0.3) {
//        chaosSynth[0].release(0);
    }
    if (mChaos > rangeStart && mChaos < rangeEnd) {
        if (rnd::prob(0.0032)) {
            chaosSynth[0].mPresetHandler.setMorphTime(3 + rnd::uniform(1.0, -1.0));
            chaosSynth[0].mPresetHandler.recallPreset(0);
        } else if (rnd::prob(0.003)) {
            chaosSynth[0].mPresetHandler.setMorphTime(3 + rnd::uniform(1.0, -1.0));
            chaosSynth[0].mPresetHandler.recallPreset(1);
        } else if (rnd::prob(0.0027)) {
            chaosSynth[0].mPresetHandler.setMorphTime(3 + rnd::uniform(1.0, -1.0));
            chaosSynth[0].mPresetHandler.recallPreset(2);
        } else if (rnd::prob(0.003)) {
            chaosSynth[0].mPresetHandler.setMorphTime(3 + rnd::uniform(1.0, -1.0));
            chaosSynth[0].mPresetHandler.recallPreset(5);
        }
    }


    for (int i = 0; i < CHAOS_SYNTH_POLYPHONY; i++) {
        if (!chaosSynth[i].done()) {
            chaosSynth[i].generateAudio(io);
            io.frame(0);
        }
    }
    addSynth.generateAudio(io);

    /// Sequences
    ///
    ///
    rangeStart = 0.1;
    rangeEnd = 0.3;
    if (mChaos > rangeStart && mChaos < rangeEnd) {
        addSynth.allNotesOff();
        mSequencer1.playSequence("Seq 1");
    }
    rangeStart = 0.3;
    rangeEnd = 0.5;
    if (mChaos > rangeStart && mChaos < rangeEnd) {
        addSynth.allNotesOff();
        mSequencer1.playSequence("Seq 2");
    }

    rangeStart = 0.2;
    rangeEnd = 0.6;
    if (mChaos > rangeStart && mChaos < rangeEnd) {
        addSynth2.allNotesOff();
        mSequencer2.playSequence("Seq 3");
    }

    for (int i = 0; i < bufferSize; i++) {
        *swBuffer++ *= 0.07;
    }

//    std::cout << "-------------" << std::endl;
//    for (int i= 0; i < 60; i++) {
//        std::cout << io.out(i, 0) << " ";
//    }
//    std::cout << std::endl;

    mDownMixer.process(io);
}

int main(int argc, char *argv[] )
{
    AudioApp app;

    int outChans = 60;
#ifdef BUILDING_FOR_ALLOSPHERE
    app.audioIO().device(AudioDevice("ECHO X5"));
#endif
    app.initAudio(44100, 4096, outChans, 0);
    gam::sampleRate(app.audioIO().fps());

//    AudioDevice::printAll();
    app.audioIO().print();
    app.init();
    app.start();
    return 0;
}
