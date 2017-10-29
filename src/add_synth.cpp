#include <iostream>
#include <vector>
#include <cmath>
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

//#define SURROUND
#include "add_synth.hpp"

#include "downmixer.hpp"

using namespace std;
using namespace al;


class PresetKeyboardControl;

class AddSynthApp: public App
{
public:
    AddSynthApp(int midiChannel) : /*mKeyboardPresets(nav()),*/ mMidiChannel(midiChannel - 1)
    {
        initializeValues();
        initWindow(Window::Dim(800,600), "Add Synth MIDI channel " + std::to_string(midiChannel));

        int outChans = 60;

        synth.outputRouting = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
            {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
            {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59} };
        audioIO().device(AudioDevice("ECHO X5"));
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

        synth.mModType.registerChangeCallback([](float value, void *sender,
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

    // Envelope
    void trigger(int id);
    void release(int id);

private:

    Parameter keyboardOffset {"Key Offset", "", 0.0, "", -20, 40};

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

    DownMixer mDownMixer;

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
                            app->synth.mFundamental.set(midi2cps(msg->at(1)));
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
                        app->synth.mPresetHandler.recallPreset(msg->at(1));
                        break;
                    default:;
                    }
                }
            }
        }
    }

    // Synthesis
    int mMidiChannel;
    AddSynth synth;
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
}

void AddSynthApp::initializeGui()
{
    gui << SequencerGUI::makePresetHandlerView(synth.mPresetHandler, 1.0, 12, 4);
    gui << synth.mLevel << synth.mFundamental << synth.mCumulativeDelay << synth.mCumulativeDelayRandomness;
    gui << synth.mArcStart << synth.mArcSpan;
	gui << synth.mAttackCurve << synth.mReleaseCurve;
    gui << synth.mLayer;
    gui << midiLight;
    gui <<keyboardOffset;

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
            app->synth.multiplyPartials(0.99);
        }
    }, glv::Update::Value, this);
    compressButton->property(glv::Momentary, true);
    harmonicsBox << compressButton << new glv::Label("Compress");

    glv::Button *expandButton = new glv::Button;
    expandButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.multiplyPartials(1/0.99);
        }
    }, glv::Update::Value, this);
    expandButton->property(glv::Momentary, true);
    harmonicsBox << expandButton << new glv::Label("Expand");

    glv::Button *randomizeButton = new glv::Button;
    randomizeButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.randomizePartials(10.0);
        }
    }, glv::Update::Value, this);
    randomizeButton->property(glv::Momentary, true);
    harmonicsBox << randomizeButton << new glv::Label("Randomize Sort");

    glv::Button *randomizeNoSortButton = new glv::Button;
    randomizeNoSortButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.randomizePartials(10.0, false);
        }
    }, glv::Update::Value, this);
    randomizeNoSortButton->property(glv::Momentary, true);
    harmonicsBox << randomizeNoSortButton << new glv::Label("Randomize");

    glv::Button *harmonicButton = new glv::Button;
    harmonicButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.harmonicPartials();
        }
    }, glv::Update::Value, this);
    harmonicButton->property(glv::Momentary, true);
    harmonicsBox << harmonicButton << new glv::Label("Harmonic");

    glv::Button *evenButton = new glv::Button;
    evenButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.oddPartials();
        }
    }, glv::Update::Value, this);
    evenButton->property(glv::Momentary, true);
    harmonicsBox << evenButton << new glv::Label("Even only");

    for (int i = 0; i < NUM_VOICES; i++) {
        harmonicsBox << ParameterGUI::makeParameterView(synth.mFrequencyFactors[i]);
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
            app->synth.setAmpsToOne();
        }
    }, glv::Update::Value, this);
    oneButton->property(glv::Momentary, true);
    amplitudesBox << oneButton << new glv::Label("All One");

    glv::Button *oneOverNButton = new glv::Button;
    oneOverNButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.setAmpsOneOverN();
        }
    }, glv::Update::Value, this);
    oneOverNButton->property(glv::Momentary, true);
    amplitudesBox << oneOverNButton << new glv::Label("One over N");

    glv::Button *slopeUpButton = new glv::Button;
    slopeUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampSlopeFactor(1.005);
        }
    }, glv::Update::Value, this);
    slopeUpButton->property(glv::Momentary, true);
    amplitudesBox << slopeUpButton << new glv::Label("Slope up");

    glv::Button *slopeDownButton = new glv::Button;
    slopeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampSlopeFactor(1/1.005);
        }
    }, glv::Update::Value, this);
    slopeDownButton->property(glv::Momentary, true);
    amplitudesBox << slopeDownButton << new glv::Label("Slope down");

    for (int i = 0; i < NUM_VOICES; i++) {
        amplitudesBox << ParameterGUI::makeParameterView(synth.mAmplitudes[i]);
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
            app->synth.attackTimeAdd(0.05);
        }
    }, glv::Update::Value, this);
    attackTimeUpButton->property(glv::Momentary, true);
    attackTimesBox << attackTimeUpButton << new glv::Label("Slower Attack");

    glv::Button *attackTimeDownButton = new glv::Button;
    attackTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.attackTimeAdd(-0.05);
        }
    }, glv::Update::Value, this);
    attackTimeDownButton->property(glv::Momentary, true);
    attackTimesBox << attackTimeDownButton << new glv::Label("Faster Attack");

    glv::Button *attackRampUpButton = new glv::Button;
    attackRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.attackTimeSlope(1.01);
        }
    }, glv::Update::Value, this);
    attackRampUpButton->property(glv::Momentary, true);
    attackTimesBox << attackRampUpButton << new glv::Label("Ramp up");

    glv::Button *attackRampDownButton = new glv::Button;
    attackRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.attackTimeSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    attackRampDownButton->property(glv::Momentary, true);
    attackTimesBox << attackRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        attackTimesBox << ParameterGUI::makeParameterView(synth.mAttackTimes[i]);
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
            app->synth.decayTimeAdd(0.05);
        }
    }, glv::Update::Value, this);
    decayTimeUpButton->property(glv::Momentary, true);
    decayTimesBox << decayTimeUpButton << new glv::Label("Slower Decay");

    glv::Button *decayTimeDownButton = new glv::Button;
    decayTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.decayTimeAdd(-0.05);
        }
    }, glv::Update::Value, this);
    decayTimeDownButton->property(glv::Momentary, true);
    decayTimesBox << decayTimeDownButton << new glv::Label("Faster Decay");

    glv::Button *decayRampUpButton = new glv::Button;
    decayRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.decayTimeSlope(1.01);
        }
    }, glv::Update::Value, this);
    decayRampUpButton->property(glv::Momentary, true);
    decayTimesBox << decayRampUpButton << new glv::Label("Ramp up");

    glv::Button *decayRampDownButton = new glv::Button;
    decayRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.decayTimeSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    decayRampDownButton->property(glv::Momentary, true);
    decayTimesBox << decayRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        decayTimesBox << ParameterGUI::makeParameterView(synth.mDecayTimes[i]);
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
            app->synth.sustainAdd(0.05);
        }
    }, glv::Update::Value, this);
    sustainTimeUpButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainTimeUpButton << new glv::Label("Higher Sus");

    glv::Button *sustainTimeDownButton = new glv::Button;
    sustainTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.sustainAdd(-0.05);
        }
    }, glv::Update::Value, this);
    sustainTimeDownButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainTimeDownButton << new glv::Label("Lower Sus");

    glv::Button *sustainRampUpButton = new glv::Button;
    sustainRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.sustainSlope(1.01);
        }
    }, glv::Update::Value, this);
    sustainRampUpButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainRampUpButton << new glv::Label("Ramp up");

    glv::Button *sustainRampDownButton = new glv::Button;
    sustainRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.sustainSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    sustainRampDownButton->property(glv::Momentary, true);
    sustainLevelsBox << sustainRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        sustainLevelsBox << ParameterGUI::makeParameterView(synth.mSustainLevels[i]);
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
            app->synth.releaseTimeAdd(0.05);
        }
    }, glv::Update::Value, this);
    releaseTimeUpButton->property(glv::Momentary, true);
    releaseTimesBox << releaseTimeUpButton << new glv::Label("Slower Release");

    glv::Button *releaseTimeDownButton = new glv::Button;
    releaseTimeDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.releaseTimeAdd(-0.05);
        }
    }, glv::Update::Value, this);
    releaseTimeDownButton->property(glv::Momentary, true);
    releaseTimesBox << releaseTimeDownButton << new glv::Label("Faster Release");

    glv::Button *releaseRampUpButton = new glv::Button;
    releaseRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.releaseTimeSlope(1.01);
        }
    }, glv::Update::Value, this);
    releaseRampUpButton->property(glv::Momentary, true);
    releaseTimesBox << releaseRampUpButton << new glv::Label("Ramp up");

    glv::Button *releaseRampDownButton = new glv::Button;
    releaseRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.releaseTimeSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    releaseRampDownButton->property(glv::Momentary, true);
    releaseTimesBox << releaseRampDownButton << new glv::Label("Ramp down");

    for (int i = 0; i < NUM_VOICES; i++) {
        releaseTimesBox << ParameterGUI::makeParameterView(synth.mReleaseTimes[i]);
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
        app->synth.mModType.setNoCalls(b->selectedItem(), b);
    }, glv::Update::Value, this);
    ampModBox << modDropDown << new glv::Label("Mod type");

    glv::Button *ampModUpButton = new glv::Button;
    ampModUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampModFreqAdd(0.05);
        }
    }, glv::Update::Value, this);
    ampModUpButton->property(glv::Momentary, true);
    ampModBox << ampModUpButton << new glv::Label("Higher freq");

    glv::Button *ampModDownButton = new glv::Button;
    ampModDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampModFreqAdd(-0.05);
        }
    }, glv::Update::Value, this);
    ampModDownButton->property(glv::Momentary, true);
    ampModBox << ampModDownButton << new glv::Label("Lower freq");

    glv::Button *ampModRampUpButton = new glv::Button;
    ampModRampUpButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampModFreqSlope(1.01);
        }
    }, glv::Update::Value, this);
    ampModRampUpButton->property(glv::Momentary, true);
    ampModBox << ampModRampUpButton << new glv::Label("Ramp up");

    glv::Button *ampModRampDownButton = new glv::Button;
    ampModRampDownButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampModFreqSlope(1/1.01);
        }
    }, glv::Update::Value, this);
    ampModRampDownButton->property(glv::Momentary, true);
    ampModBox << ampModRampDownButton << new glv::Label("Ramp down");

    ampModBox << ParameterGUI::makeParameterView(synth.mModDepth);
    ampModBox << ParameterGUI::makeParameterView(synth.mModAttack);
    ampModBox << ParameterGUI::makeParameterView(synth.mModRelease);

	glv::Button *randomizeAmpModButton = new glv::Button;
    randomizeAmpModButton->attach([](const glv::Notification &n) {
        glv::Button *b = n.sender<glv::Button>();
        AddSynthApp *app = n.receiver<AddSynthApp>();
        if (b->getValue() == 1) {
            app->synth.ampModRandomize(10.0, false);
        }
    }, glv::Update::Value, this);
    randomizeAmpModButton->property(glv::Momentary, true);
    ampModBox << randomizeAmpModButton << new glv::Label("Randomize Sort");

    for (int i = 0; i < NUM_VOICES; i++) {
        ampModBox << ParameterGUI::makeParameterView(synth.mAmpModFrequencies[i]);
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
//    mPresetHandler.print();
    sequencer << synth.mPresetHandler;
    recorder << synth.mPresetHandler;
//    mKeyboardPresets.presets = &mPresetHandler;

    // MIDI Control of parameters
    int midiPort = 0;
    parameterMIDI.init(midiPort);
    parameterMIDI.connectControl(synth.mCumulativeDelay, 75, 1);
    parameterMIDI.connectControl(synth.mCumulativeDelayRandomness, 76, 1);
    parameterMIDI.connectControl(synth.mArcStart, 77, 1);
    parameterMIDI.connectControl(synth.mArcSpan, 78, 1);

    // MIDI control of presets
    // 74 71 91 93 73 72 5 84 7
    // 75 76 77 78 74 71 24 102
    presetMIDI.init(midiPort, synth.mPresetHandler);
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
    synth.generateAudio(io);
    mDownMixer.process(io);
}

inline float mtof(int m) {
    return 440.0 * pow (2, (m - 69)/ 12.0);
}

void AddSynthApp::onKeyDown(const Keyboard &k)
{
    if (!mPresetKeyboardActive) {
        return;
    }
//    if (k.ctrl() || k.alt() || k.meta()) {
//        trigger(100);
//    }

    float frequency = 440;
    int keyOffset = (int) keyboardOffset.get();
    int id = k.key();
    switch(id){
    case '1': frequency = mtof(50 + keyOffset);break;
    case '2': frequency = mtof(51 + keyOffset);break;
    case '3': frequency = mtof(52 + keyOffset);break;
    case '4': frequency = mtof(53 + keyOffset);break;
    case '5': frequency = mtof(54 + keyOffset);break;
    case '6': frequency = mtof(55 + keyOffset);break;
    case '7': frequency = mtof(56 + keyOffset);break;
    case '8': frequency = mtof(57 + keyOffset);break;
    case '9': frequency = mtof(58 + keyOffset);break;
    case '0': frequency = mtof(59 + keyOffset);break;
    case 'q': frequency = mtof(60 + keyOffset);break;
    case 'w': frequency = mtof(61 + keyOffset);break;
    case 'e': frequency = mtof(62 + keyOffset);break;
    case 'r': frequency = mtof(63 + keyOffset);break;
    case 't': frequency = mtof(64 + keyOffset);break;
    case 'y': frequency = mtof(65 + keyOffset);break;
    case 'u': frequency = mtof(66 + keyOffset);break;
    case 'i': frequency = mtof(67 + keyOffset);break;
    case 'o': frequency = mtof(68 + keyOffset);break;
    case 'p': frequency = mtof(69 + keyOffset);break;
    case 'a': frequency = mtof(70 + keyOffset);break;
    case 's': frequency = mtof(71 + keyOffset);break;
    case 'd': frequency = mtof(72 + keyOffset);break;
    case 'f': frequency = mtof(73 + keyOffset);break;
    case 'g': frequency = mtof(74 + keyOffset);break;
    case 'h': frequency = mtof(75 + keyOffset);break;
    case 'j': frequency = mtof(76 + keyOffset);break;
    case 'k': frequency = mtof(77 + keyOffset);break;
    case 'l': frequency = mtof(78 + keyOffset);break;
    case ';': frequency = mtof(79 + keyOffset);break;
    case 'z': frequency = mtof(80 + keyOffset);break;
    case 'x': frequency = mtof(81 + keyOffset);break;
    case 'c': frequency = mtof(82 + keyOffset);break;
    case 'v': frequency = mtof(83 + keyOffset);break;
    case 'b': frequency = mtof(84 + keyOffset);break;
    case 'n': frequency = mtof(85 + keyOffset);break;
    case 'm': frequency = mtof(86 + keyOffset);break;
    case ',': frequency = mtof(87 + keyOffset);break;
    case '.': frequency = mtof(88 + keyOffset);break;
    case '/': frequency = mtof(89 + keyOffset);break;
    }

//    std::cout << id << ".." << frequency << std::endl;
    synth.mFundamental.set(frequency);
    trigger(id);
}

void AddSynthApp::onKeyUp(const Keyboard &k)
{
    if (!mPresetKeyboardActive) {
        return;
    }
    release(k.key());
}

void AddSynthApp::trigger(int id)
{
//    std::cout << "trigger id " << id << std::endl;
    synth.trigger(id);
}

void AddSynthApp::release(int id)
{
    synth.release(id);
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
