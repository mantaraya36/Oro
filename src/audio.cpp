
#include <vector>

#include "allocore/al_Allocore.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "allocore/ui/al_Preset.hpp"
#include "allocore/ui/al_PresetMIDI.hpp"
#include "allocore/ui/al_ParameterMIDI.hpp"
#include "allocore/io/al_MIDI.hpp"

#define SURROUND

#include "chaos_synth.hpp"
#include "add_synth.hpp"
#include "granulator.hpp"

#define CHAOS_SYNTH_POLYPHONY 1
#define ADD_SYNTH_POLYPHONY 1

using namespace std;
using namespace al;

class DownMixer {
public:
    DownMixer() {
        mDownMixMap[0] = vector<int>();
        mDownMixMap[1] = vector<int>();
        for (int i = 0; i < 60; i++) {
            mDownMixMap[0].push_back(i);
            mDownMixMap[1].push_back(i);
        }
        mGain = 1/30.0;
    }

    void process(AudioIOData &io) {
        int counter;
        for (auto &entry: mDownMixMap) {
            for (int sourceIndex: entry.second) {
                float *destBuffer = io.outBuffer(entry.first);
                float *srcBuffer = io.outBuffer(sourceIndex);
                counter = io.framesPerBuffer();
                while (counter--) {
                    *destBuffer++ += *srcBuffer++;
                }
            }
            float *destBuffer = io.outBuffer(entry.first);
            counter = io.framesPerBuffer();
            while (counter--) {
                *destBuffer++ *= mGain;
            }
        }
    }

private:
    map<int, vector<int>> mDownMixMap;
    float mGain;
};

class AudioApp: public App, public osc::PacketHandler
{
public:
    AudioApp(int midiChannel = 1) : App(),
        mMidiChannel(midiChannel - 1),
        granX("Bounced Files/Piezas oro 1.wav"),
        granY("Bounced Files/Piezas oro 2.wav"),
        granZ("Bounced Files/Piezas oro 2.wav"),
        background1("Bounced Files/Agua base superficie.wav"),
	    background2("Bounced Files/Bajo agua.wav")
    {
        addSynth.outputRouting = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
            {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59}
        };
        mOSCRecv.handler(*this);
        mOSCRecv.timeout(0.005);
        mOSCRecv.start();

    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    virtual void onSound(AudioIOData &io) override;
    virtual void onMessage(osc::Message &m) override { m.print();}

private:

    // Parameters
    Parameter mLevel {"Level", "", 0.5, "", 0, 1.0};
    Parameter mFundamental {"Fundamental", "", 55.0, "", 0.0, 9000.0};

    Parameter keyboardOffset {"Key Offset", "", 0.0, "", -20, 40};

//    // Presets
//    PresetHandler mChaosPresetHandler {"chaosPresets"};
//    PresetHandler mAddPresetHandler {"presets"};

    // MIDI Control
    PresetMIDI presetMIDI;
    ParameterMIDI parameterMIDI;
    int mMidiChannel;

    MIDIIn midiIn {"USB Oxygen 49"};


    static void midiCallback(double deltaTime, std::vector<unsigned char> *msg, void *userData){
        AudioApp *app = static_cast<AudioApp *>(userData);
        unsigned numBytes = msg->size();
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
                            app->addSynth.trigger(msg->at(1));
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
//                        app->mPresetHandler.recallPreset(msg->at(1));
                        break;
                    default:;
                    }
                }
            }
        }
    }

    // Synthesis
    ChaosSynth chaosSynth[CHAOS_SYNTH_POLYPHONY];
    AddSynth addSynth;
    Granulator granX, granY, granZ;
    Granulator background1;
    Granulator background2;
//    Granulator background3;

    gam::SineR<float> fluctuation1, fluctuation2;

    // Schedule Messages
    MsgQueue msgQueue;

    DownMixer mDownMixer;
};

static void releaseAddSynth(al_sec timestamp, AddSynth *addSynth, int id)
{
    addSynth->release(id);
    std::cout << "release" << std::endl;
}

void AudioApp::onSound(AudioIOData &io)
{
    msgQueue.advance(io.framesPerBuffer()/io.framesPerSecond());
    if (rnd::prob(0.0085)) {
        std::cout << "trigger" << std::endl;
        addSynth.mPresetHandler.recallPreset("34");
        addSynth.mLayer = 0;
        addSynth.mArcSpan = rnd::uniform(0.5, 2.0);
        addSynth.mArcStart = rnd::uniform();
        addSynth.mCumulativeDelayRandomness = addSynth.mCumulativeDelayRandomness +rnd::uniform(0.1, -0.1);
        addSynth.mFundamental = addSynth.mFundamental + rnd::uniform(30, -30);
        addSynth.trigger(0);
        msgQueue.send(msgQueue.now() + 2.5, releaseAddSynth, &addSynth, 0);
    }
    if (rnd::prob(0.0082)) {
        std::cout << "trigger" << std::endl;
        addSynth.mPresetHandler.recallPreset("gliss");
        addSynth.mLayer = 0;
        addSynth.mArcSpan = rnd::uniform(0.5, 2.0);
        addSynth.mArcStart = rnd::uniform();
        addSynth.mCumulativeDelayRandomness = addSynth.mCumulativeDelayRandomness +rnd::uniform(0.1, -0.1);
        addSynth.mFundamental = addSynth.mFundamental + rnd::uniform(30, -30);
        addSynth.trigger(0);
        msgQueue.send(msgQueue.now() + 2.5, releaseAddSynth, &addSynth, 0);
    }
    for (int i = 0; i < CHAOS_SYNTH_POLYPHONY; i++) {
        if (!chaosSynth[i].done()) {
            chaosSynth[i].generateAudio(io);
            io.frame(0);
        }
    }
    addSynth.generateAudio(io);

    // Sample playback
    float out1level = 0.01f;//* mSpeedX * mouseSpeedScale;
    float out2level = 0.01f;//* mSpeedX * mouseSpeedScale;

    float out1 = granX() * out1level;
    float out2 = granY() * out2level;

    float bg1level = 0.01f;
    float bg2level = 0.01f;
    float bg3level = 0.01f;
    float bg1 = background1() * bg1level;
    float bg2 = background2() * bg2level;
//    float bg3 = background3() * bg3level;
    float fluct1 = fluctuation1();
    float fluct2 = fluctuation2();
    io.out(19) += out1 + (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0);
    io.out(27) += out2 + (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct1)/2.0);

    io.out(45) += (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0);
    io.out(31) += (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct2)/2.0);
    io.out(40) += out1 + (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0);
    io.out(36) += out2 + (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct1)/2.0);

    mDownMixer.process(io);

}

int main(int argc, char *argv[] )
{
    int midiChannel = 1;
    if (argc > 1) {
        midiChannel = atoi(argv[1]);
    }

    AudioApp app(midiChannel);

//    app.initWindow();
    int outChans = 60;
//#ifdef SURROUND
//    int outChans = 60;
//#else
//    int outChans = 2;
//#endif
    app.initAudio(44100, 2048, outChans, 0);
    gam::sampleRate(app.audioIO().fps());

//    AudioDevice::printAll();
    app.audioIO().print();

    app.start();
    return 0;
}
