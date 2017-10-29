
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
        for (auto baseNames : mBasesFilenames) {
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
    }

    static inline float midi2cps(int midiNote) {
        return 440.0 * pow(2, (midiNote - 69.0)/ 12.0);
    }

    void init() {

        mChaosRecv.handler(*this);
        mChaosRecv.timeout(0.005);
        mChaosRecv.start();
    }

    virtual void onSound(AudioIOData &io) override;
    virtual void onMessage(osc::Message &m) override {
        if (m.addressPattern() == "/chaos" && m.typeTags() == "f") {
            m >> mChaos;
        }
        m.print();
    }

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

    std::vector<int> mBaseRouting = {49, 58, 52, 55,23,26, 30, 34 , 38, 42, 16 , 20 , 1, 10, 4, 7 };

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

    std::vector<std::string> mBasesFilenames {
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


    // Schedule Messages
    MsgQueue msgQueue;

    DownMixer mDownMixer;

    osc::Recv mChaosRecv {AUDIO_IN_PORT, AUDIO_IP_ADDRESS};

    float mChaos {0};
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

    std::vector<float> mCamasGains = {1.0, 1.0, 1.0, 1.0, 1.0};

    int fileIndex = 0;
    if (mChaos < 0.2) {
        fileIndex = 0;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex]);
    } else if (mChaos < 0.3) {
        float gainIndex = (mChaos - 0.2) * 10;

        fileIndex = 0;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 1;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] *gainIndex);

    }  else if (mChaos < 0.4) {
        fileIndex = 1;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex]);

    } else if (mChaos < 0.5) {
        float gainIndex = (mChaos - 0.4) * 10;

        fileIndex = 1;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 2;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] *gainIndex);

    } else if (mChaos < 0.6) {

        fileIndex = 2;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex]);

    } else if (mChaos < 0.7) {

        float gainIndex = (mChaos - 0.6) * 10;

        fileIndex = 2;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 3;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] *gainIndex);


    } else if (mChaos < 0.8) {

        fileIndex = 3;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex]);


    }  else if (mChaos < 0.9) {

        float gainIndex = (mChaos - 0.8) * 10;

        fileIndex = 3;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] * (1.0 - gainIndex));

        fileIndex = 4;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex] *gainIndex);

    } else {
        fileIndex = 4;
        readFile(mCamaFiles[fileIndex], readBuffer, io, mBaseRouting, mCamasGains[fileIndex]);

    }

    for (int i = 0; i < mVoices.size(); i++) {
        assert(bufferSize < 8192);
        if (mVoices[i]->read(readBuffer, bufferSize) == bufferSize) {
            float *buf = readBuffer;
            float *bufsw = swBuffer;
            float *outbuf = io.outBuffer(mVoicesRouting[i]);
            while (io()) {
                float out = *buf++ * 0.3;
                *outbuf++ += out;
                *bufsw++ += out;
            }
            io.frame(0);
        } else {
            //                    std::cout << "Error" << std::endl;
        }
    }

    msgQueue.advance(io.framesPerBuffer()/io.framesPerSecond());
    if (rnd::prob(0.0075)) {
        std::cout << "trigger" << std::endl;
        addSynth.mPresetHandler.recallPreset("34");
        addSynth.mLayer = 1;
        addSynth.mArcSpan = rnd::uniform(0.5, 2.0);
        addSynth.mArcStart = rnd::uniform();
        addSynth.mCumulativeDelayRandomness = addSynth.mCumulativeDelayRandomness +rnd::uniform(0.2, -0.2);
        addSynth.mFundamental = addSynth.mFundamental + rnd::uniform(40, -40);
        addSynth.trigger(0);
        msgQueue.send(msgQueue.now() + 2.5, releaseAddSynth, &addSynth, 0);
    }
    if (rnd::prob(0.0079)) {
//        std::cout << "trigger" << std::endl;
//        addSynth.mPresetHandler.recallPreset("gliss");
//        addSynth.mLayer = 1;
//        addSynth.mArcSpan = rnd::uniform(0.5, 2.0);
//        addSynth.mArcStart = rnd::uniform();
//        addSynth.mCumulativeDelayRandomness = addSynth.mCumulativeDelayRandomness +rnd::uniform(0.2, -0.2);
//        addSynth.mFundamental = addSynth.mFundamental + rnd::uniform(40, -40);
//        addSynth.trigger(0);
//        msgQueue.send(msgQueue.now() + 2.5, releaseAddSynth, &addSynth, 0);
    }
    // Chaos Synth triggers
    if (mChaos > 0.3 && mChaos < 0.7) {
        if (rnd::prob(0.0082)) {
            if (chaosSynth[0].done()) {
                chaosSynth[0].setOutputIndeces(rnd::uniform(47, 16),rnd::uniform(47, 16));
                chaosSynth[0].trigger(0);
                msgQueue.send(msgQueue.now() + 7, releaseChaosSynth, &chaosSynth[0], 0);
            }
        }
    }

    for (int i = 0; i < CHAOS_SYNTH_POLYPHONY; i++) {
        if (!chaosSynth[i].done()) {
            chaosSynth[i].generateAudio(io);
            io.frame(0);
        }
    }
    addSynth.generateAudio(io);

//    // Sample playback
//    float out1level = 0.01f;//* mSpeedX * mouseSpeedScale;
//    float out2level = 0.01f;//* mSpeedX * mouseSpeedScale;

//    float out1 = granX() * out1level;
//    float out2 = granY() * out2level;

//    float bg1level = 0.01f;
//    float bg2level = 0.01f;
//    float bg3level = 0.01f;
//    float bg1 = background1() * bg1level;
//    float bg2 = background2() * bg2level;
////    float bg3 = background3() * bg3level;
//    float fluct1 = fluctuation1();
//    float fluct2 = fluctuation2();
//    io.out(19) += out1 + (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0);
//    io.out(27) += out2 + (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct1)/2.0);

//    io.out(45) += (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0);
//    io.out(31) += (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct2)/2.0);
//    io.out(40) += out1 + (bg1 * (1 - fluct2)/2.0) + (bg2 * (1 - fluct2)/2.0);
//    io.out(36) += out2 + (bg1 * (1 - fluct1)/2.0) + (bg2 * (1 - fluct1)/2.0);

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
#ifdef BUILDING_FOR_ALLOSPHERE
    app.audioIO().device(AudioDevice("ECHO X5"));
#endif
    app.initAudio(44100, 512, outChans, 0);
    gam::sampleRate(app.audioIO().fps());

//    AudioDevice::printAll();
    app.audioIO().print();
//    app.init();
    app.start();
    return 0;
}
