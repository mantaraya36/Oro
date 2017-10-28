#ifndef RENDER_TREE_HPP
#define RENDER_TREE_HPP

#include <vector>
#include <string>
#include <iostream>
#include <memory>
#include <mutex>
#include <map>
#include <inttypes.h>
#include <cassert>

#include "allocore/graphics/al_Graphics.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/types/al_Color.hpp"
#include "allocore/graphics/al_Shapes.hpp"
#include "allocore/graphics/al_Font.hpp"

#include "allocore/protocol/al_OSC.hpp"
#include "allocore/ui/al_Parameter.hpp"

namespace al {

class RenderModule;

class Behavior {
public:
    Behavior() {}
    virtual ~Behavior() {}
    virtual void init() = 0;
    virtual void tick() = 0;

    void setModule(RenderModule *module) {
        mModule = module;
    }
    bool done() {return mDone;}

protected:
    bool mDone = false;
    RenderModule *mModule {nullptr};
};

class Relayer {
public:

	void relay(std::string addr) {
		osc::Packet p;
		p.addMessage(addr);
		relay(p);
	}

	template <class T>
	void relay(std::string &addr, T &value) {
		osc::Packet p;
		p.addMessage(addr, value);
		relay(p);
	}

	template <class T1, class T2>
	void relay(std::string addr, T1 &value1, T2 &value2) {
		osc::Packet p;
		p.addMessage(addr, value1, value2);
		relay(p);
	}

	template <class T1, class T2, class T3>
	void relay(std::string addr, T1 &value1, T2 &value2, T3 &value3) {
		osc::Packet p;
		p.addMessage(addr, value1, value2, value3);
		relay(p);
	}

	template <class T1, class T2, class T3, class T4>
	void relay(std::string addr, T1 &value1, T2 &value2, T3 &value3, T4 &value4) {
		osc::Packet p;
		p.addMessage(addr, value1, value2, value3, value4);
		relay(p);
	}

	void relay(std::string addr, Vec3f &vec) {
		osc::Packet p;
		p.addMessage(addr, vec[0], vec[1], vec[2]);
		relay(p);
	}

	void relay(std::string addr, Vec4f &vec) {
		osc::Packet p;
		p.addMessage(addr, vec[0], vec[1], vec[2], vec[3]);
		relay(p);
	}

	void relay(osc::Packet &p) {
		for (auto &sender: mSenders) {
			sender->send(p);
		}
	}

	virtual void addRelayAddress(std::string address, int port) {
		mSenders.push_back(std::unique_ptr<osc::Send>(new osc::Send(port, address.c_str())));
	}

private:
	std::vector<std::unique_ptr<osc::Send>> mSenders;
};

class RenderModule : public OSCNotifier {
    friend class RenderTree;
    friend class RenderTreeHandler;
public:
    RenderModule() {} // perhaps have non public constructor and factory function in chain?
    virtual ~RenderModule() { relayDestruction();}

    void setPosition(Vec3f pos) { mPosition = pos; relay(moduleAddress() + "/setPosition", pos);}
    void setRotation(Vec3f rot) { mRotation = rot; relay(moduleAddress() + "/setRotation", rot);}
    void setScale(Vec3f scale) { mScale = scale; relay(moduleAddress() + "/setScale", scale);}
    void setColor(Color color) {
		mColor = color;
		float * comp = color.components;
		relay(moduleAddress() + "/setColor", comp[0], comp[1], comp[2], comp[3]);
	}
    void setId(u_int32_t id) { mId = id;}

    std::string getType() { return mType; }

    Vec3f &getPosition() { return mPosition; }
    Vec3f &getRotation() { return mRotation; }
    Vec3f &getScale() { return mScale; }
    Color &getColor() { return mColor; }
    u_int32_t &getId() { return mId; }

    unsigned long getTicks() { return mTicks; }

    void setFlag(std::string flagName, bool value) { mFlags[flagName] = value; }
    bool getFlag(std::string flagName) { if (mFlags.find(flagName) == mFlags.end()) return false; else return mFlags[flagName];}

    void addChild(std::shared_ptr<RenderModule> child) {
        std::lock_guard<std::mutex> locker(mModuleLock);
		child->setRelayer(mRelayer);
        mChildren.push_back(child);
    }

    void addBehavior(std::shared_ptr<Behavior> behavior) {
        std::lock_guard<std::mutex> locker(mModuleLock);
        behavior->setModule(this);
        behavior->init();
        mBehaviors.push_back(behavior);
    }

	void setRelayer(Relayer *relayer) {
		std::lock_guard<std::mutex> locker(mModuleLock);
		mRelayer = relayer;
		relayCreation();

		for (auto child : mChildren) {
			child->setRelayer(mRelayer);
		}
	}

    virtual void executeCommand(std::string command, std::vector<std::string> arguments)
    {
        if (command == "setPosition") {
            if (arguments.size() == 3) {
                Vec3f newPosition = Vec3f(std::stod(arguments[0]), std::stod(arguments[1]), std::stod(arguments[2]));
                setPosition(newPosition);
            }
        } else if (command == "setColor") {
            if (arguments.size() == 1) {
                Color newColor(std::stod(arguments[0]));
                setColor(newColor);
            } else if (arguments.size() == 3) {
                Color newColor(std::stod(arguments[0]), std::stod(arguments[1]), std::stod(arguments[2]));
                setColor(newColor);
            } else if (arguments.size() == 4) {
                Color newColor(std::stod(arguments[0]), std::stod(arguments[1]), std::stod(arguments[2]), std::stod(arguments[3]));
                setColor(newColor);
            }
        } else if (command == "setScale") {
            if (arguments.size() == 1) {
                Vec3f newScale = Vec3f(std::stod(arguments[0]), std::stod(arguments[0]), std::stod(arguments[0]));
                setScale(newScale);
            }
            if (arguments.size() == 3) {
                Vec3f newScale = Vec3f(std::stod(arguments[0]), std::stod(arguments[1]), std::stod(arguments[2]));
                setScale(newScale);
            }
        } else if (command == "done") {
			setDone(true);
        } else if (command == "destroy") {
			setDone(true);
        } /*else if (command == "setRotation") {

        }*/
    }

    virtual bool done() { return mDone; } // Lets render tree know it's time to remove this node
    virtual void setDone(bool done) { mDone = done; }

	std::string moduleAddress() {
		assert(mId != UINT32_MAX);
		return "/" + std::to_string(mId);
	}

protected:
    virtual void init(Graphics &g) = 0;
    virtual void render(Graphics &g, float dt = -1.0) = 0;
    virtual void cleanup() {}

	void relay(std::string addr) {
		if (mRelayer) {
			mRelayer->relay(addr);
		}
	}

	void relay(std::string addr, Vec3f vec) {
		if (mRelayer) {
			mRelayer->relay(addr, vec);
		}
	}

	void relay(std::string addr, Vec4f vec) {
		if (mRelayer) {
			mRelayer->relay(addr, vec);
		}
	}

	template<class T1>
	void relay(std::string addr, const T1 &value) {
		if (mRelayer) {
			mRelayer->relay(addr, value);
		}
	}

	template<class T1, class T2>
	void relay(std::string addr, const T1 &value, const T2 &value2) {
		if (mRelayer) {
			mRelayer->relay(addr, value, value2);
		}
	}

	template<class T1, class T2, class T3>
	void relay(std::string addr, const T1 &value, const T2 &value2, const T3 &value3) {
		if (mRelayer) {
			mRelayer->relay(addr, value, value2, value3);
		}
	}

	template<class T1, class T2, class T3, class T4>
	void relay(std::string addr, const T1 &value, const T2 &value2, const T3 &value3, const T4 &value4) {
		if (mRelayer) {
			mRelayer->relay(addr, value, value2, value3, value4);
		}
	}

	virtual void relayCreation() {
		if (mRelayer) {
			mRelayer->relay("/create", mType, mId);
		}
	}

	virtual void relayDestruction() {
		relay(moduleAddress() + "/destroy");
//		if (mRelayer) {
//			mRelayer->relay("/destroy", mId);
//		}
	}

//	static std::string moduleClassName(); // Don't override this, it will be declared by the REGISTER_MODULE macro
//	static std::shared_ptr<RenderModule> create(); // Don't override this, it will be declared by the REGISTER_MODULE macro

	// Only use this lock to protect data when setting it in child classes. The initInternal()
	// and renderInternal() functions take care of locking it for you
	std::mutex mModuleLock;

    std::string mType = "";
    bool mDone {false};
    unsigned long mTicks {0};

private:
    void initInternal(Graphics &g) {
		std::lock_guard<std::mutex> locker(mModuleLock);
        init(g);
        for(auto child : mChildren) { // TODO add protections for the case where parent has already intialized and child is added
            child->initInternal(g);
        }
    }

    void renderInternal(Graphics &g, float dt = -1.0) {
		std::lock_guard<std::mutex> locker(mModuleLock);
//        We should do some form of depth sorting here? It might help with occlusion in many cases...
        g.blending(true);
        g.blendAdd();
        g.pushMatrix();
        g.color(mColor);
        g.translate(mPosition[0], mPosition[1], mPosition[2]);
        g.scale(mScale);
        render(g, dt);
        g.popMatrix();
        for(auto child : mChildren) {
            g.pushMatrix();
            g.color(mColor);
            g.translate(mPosition[0], mPosition[1], mPosition[2]);
    //        g.rotate();

//            g.scale(mScale);
            child->renderInternal(g, dt);
            g.popMatrix();
        }
        std::vector<std::shared_ptr<Behavior>> behaviorsToRemove;
        for (auto behavior: mBehaviors) {
            if (behavior->done()) {
                behaviorsToRemove.push_back(behavior);
            } else {
                behavior->tick();
            }
        }
        for(auto behavior:behaviorsToRemove) {
            mBehaviors.erase(std::find(mBehaviors.begin(),mBehaviors.end(), behavior));
        }
        mTicks++;
    }

	Relayer *mRelayer {nullptr};

    Vec3f mPosition {0, 0, 0};
    Vec3f mRotation {0, 0, 0};
    Vec3f mScale {1, 1, 1};
    Color mColor {1.0,1.0,1.0,1.0};
//    u_int32_t mModuleType {0};
    std::vector<std::shared_ptr<RenderModule>> mChildren;
    std::vector<std::shared_ptr<Behavior>> mBehaviors;
	std::map<std::string, bool> mFlags;
	u_int32_t mId {UINT32_MAX};
};

class Timeout : public Behavior {
    friend class RenderModule;
public:
    Timeout(unsigned long numTicks) : Behavior() {
        mNumTicks = numTicks;
    }

    virtual void init() override {}

    virtual void tick() override {
        if (mNumTicks-- == 0) {
            mModule->setDone(true);
        }
    }

private:
    unsigned long mNumTicks;
};

// Behaviors ------------------------

class FadeOut : public Behavior {
    friend class RenderModule;
public:
    FadeOut(unsigned long numTicks, unsigned long delayTicks = 0) : Behavior() {
        mNumTicks = numTicks;
        mDelayTicks = delayTicks;
    }

    virtual void init() override {
        mAlphaDec = mModule->getColor().a/(float)mNumTicks;
    }

    virtual void tick() override {
        if (mDelayTicks == 0) {
			Color &c = mModule->getColor();
            c.a -= mAlphaDec;
			mModule->setColor(c);
            if (mNumTicks-- == 0) {
                mModule->setDone(true);
            }
        } else {
            mDelayTicks--;
        }
    }

private:
    float mAlphaDec {0};
    unsigned long mNumTicks;
    unsigned long mDelayTicks;
};

class FadeIn : public Behavior {
    friend class RenderModule;
public:
    FadeIn(unsigned long numTicks) : Behavior() {
        mNumTicks = numTicks;
    }

    virtual void init() override {
        mAlphaInc = mModule->getColor().a/(float)mNumTicks;
    }

    virtual void tick() override {
        Color &c = mModule->getColor();
		c.a += mAlphaInc;
		mModule->setColor(c);
        if (mNumTicks-- == 0) {
            mDone = true;
        }
    }

private:
    float mAlphaInc {0};
    unsigned long mNumTicks;
};

class Sink : public Behavior {
    friend class RenderModule;
public:
    Sink(unsigned long numTicks, float zDelta) : Behavior() {
        mNumTicks = numTicks;
        mDelta = zDelta/numTicks;
    }

    virtual void init() override {
        mTargetTicks = mModule->getTicks() + mNumTicks;
    }

    virtual void tick() override {
        if (mModule->getTicks() < mTargetTicks) {
            Vec3f pos = mModule->getPosition();
            pos.z += mDelta;
            mModule->setPosition(pos);
		} else {
			mDone = true;
		}
    }

private:
    unsigned long mTargetTicks;
    unsigned long mNumTicks;
    float mDelta;
};

// Yes this is absolutely gross. Please fix it if you can!

static std::map<std::string, std::function<std::shared_ptr<RenderModule> ()>> __renderModuleMap;

static bool registerModule(std::string moduleName, std::function<std::shared_ptr<RenderModule>()> creator)
{
	__renderModuleMap[moduleName] = creator;
	return true;
}

#define REGISTER_MODULE(MODULETYPENAME) bool _Registered_##MODULETYPENAME = registerModule(#MODULETYPENAME, MODULETYPENAME::createBase);

// Modules
class TextRenderModule : public RenderModule {
public:
    TextRenderModule() { mType = "TextRenderModule"; } // perhaps have non public constructor and factory function in chain?

    static std::shared_ptr<TextRenderModule> create() { return std::make_shared<TextRenderModule>();}
	static std::shared_ptr<RenderModule> createBase() { return std::static_pointer_cast<RenderModule>(create());}

    void loadFont(const std::string& filename, int fontSize=10, bool antialias=true)
    {
        mFont.load(filename, fontSize, antialias);
		relay(moduleAddress() + "/loadFont", filename, fontSize, antialias ? 1: 0);
    }

    void setText(std::string text) {
        mText = text;
        if (initDone) {
            mFont.write(mTextMesh, mText);
        }
		relay(moduleAddress() + "/setText", text);
    }
    void setFontSize(float size) {
        mFontSize = size;
//        mFont.size(size);
        loadFont(mFontPath, mFontSize);
    }

    virtual void executeCommand(std::string command, std::vector<std::string> arguments) override
    {
        RenderModule::executeCommand(command, arguments);
		if (command == "loadFont") {
            if (arguments.size() > 0) {
                loadFont(arguments.at(0), std::atoi(arguments.at(1).c_str()), arguments.at(1) == "1");
            }
        } else if (command == "setText") {
            if (arguments.size() > 0) {
                setText(arguments.at(0));
            }
        } else if (command == "setFontSize") {
            if (arguments.size() > 0) {
                setFontSize(std::stoi(arguments.at(0)));
            }
        }
    }

protected:
    virtual void init(Graphics &g)
    {
        mFont.write(mTextMesh, mText);
        initDone = true;
    }

    virtual void render(Graphics &g, float dt = 1.0f)
    {
        g.pushMatrix();
        mFont.texture().bind();
        g.scale(0.005);
        g.draw(mTextMesh);
        mFont.texture().unbind();
        g.popMatrix();
    }


//    virtual void cleanup() {}

private:
    std::string mText {"Test"};
    std::string mFontPath {"AlloSystem/allocore/share/fonts/VeraMono.ttf"};
    float mFontSize {18};
    Font mFont{mFontPath, (int)mFontSize};
    Mesh mTextMesh;

    bool initDone {false};

};

REGISTER_MODULE(TextRenderModule);

class LineStripModule : public RenderModule {
public:
    LineStripModule() {
        mType = "LineStripModule";
        mMesh.reset();
        mMesh.primitive(Graphics::TRIANGLE_STRIP);
        mMesh.vertices().resize(mMaxLen * 2);
    }

    static std::shared_ptr<LineStripModule> create() { return std::make_shared<LineStripModule>();}
	static std::shared_ptr<RenderModule> createBase() { return std::static_pointer_cast<RenderModule>(create());}

    void setDelta(float delta) { mDelta = delta; }
    void setThickness(float thickness) { mThickness = thickness; }

    void addValue(float value)
    {
        std::lock_guard<std::mutex> locker(mModuleLock);
        if (mNumVertices >= mMaxLen* 2) {
            std::cout << "Can't add more values to line strip. mMaxLen = " << mMaxLen << std::endl;
            return;
        }
        mMesh.vertices()[mNumVertices++] =  {value, mNumVertices* mDelta, 0};
        mMesh.vertices()[mNumVertices++] =  {value+ mThickness, mNumVertices* mDelta, mThickness};
    }

    void addVertex(float x, float y, float z)
    {
        std::lock_guard<std::mutex> locker(mModuleLock);
        if (mNumVertices >= mMaxLen* 2) {
            std::cout << "Can't add more values to line strip. mMaxLen = " << mMaxLen << std::endl;
            return;
        }
        mMesh.vertices()[mNumVertices++] =  {x, y, z};
        mMesh.vertices()[mNumVertices++] =  {x+ mThickness, y, z + mThickness};
    }

protected:

    virtual void init(Graphics &g) override
    {
    }

    virtual void render(Graphics &g, float dt = 1.0f) override
    {
//        mMesh.reset();
//        addCube(mMesh);
        g.blendTrans();
        mMesh.primitive(Graphics::TRIANGLE_STRIP);
        g.draw(mMesh, mNumVertices);
    }

private:
    unsigned int mNumVertices {0};
    size_t mMaxLen {128};
    size_t mBegin {0}, mEnd{0};
    float mDelta {0.1f};
    float mThickness {0.2f};

    Mesh mMesh;
};

REGISTER_MODULE(LineStripModule);

class ImageRenderModule : public RenderModule {
public:
    ImageRenderModule() {
        mType = "ImageRenderModule";
    } // Non public constructor?
    static std::shared_ptr<ImageRenderModule> create() { return std::make_shared<ImageRenderModule>();}
	static std::shared_ptr<RenderModule> createBase() { return std::static_pointer_cast<RenderModule>(create());}

    void loadImage(std::string filename)
    {
        std::lock_guard<std::mutex> locker(mModuleLock);
        mImage.load(filename);
        // TODO have debugging output that can be enabled/disabled
        std::cout << "Read image from " << filename << "  " << mImage.format() << std::endl;
        mTexture.allocate(mImage.array());
        mTexture.submit();
    }

protected:
    virtual void init(Graphics &g) override
    {
        mQuad.reset();
        mQuad.primitive(Graphics::TRIANGLE_STRIP);
        mQuad.texCoord(0, 0);
        mQuad.vertex(-1, -1, 0);
        mQuad.texCoord(1, 0);
        mQuad.vertex( 1, -1,0);
        mQuad.texCoord(0, 1);
        mQuad.vertex(-1,  1, 0);
        mQuad.texCoord(1, 1);
        mQuad.vertex( 1,  1, 0);
    }

    virtual void render(Graphics &g, float dt = 1.0f) override
    {
        g.blendModeTrans();
        mTexture.bind(0);
        g.draw(mQuad);
        mTexture.unbind();
    }

private:
    Texture mTexture;
    Image mImage;
    Mesh mQuad;
};

REGISTER_MODULE(ImageRenderModule);

class MeshModule : public RenderModule {
public:
    MeshModule() {
        mType = "MeshModule";
    } // Non public constructor?
    static std::shared_ptr<MeshModule> create() { return std::make_shared<MeshModule>();}
	static std::shared_ptr<RenderModule> createBase() { return std::static_pointer_cast<RenderModule>(create());}

protected:
    virtual void init(Graphics &g) override
    {
        mMesh.reset();
        mMesh.primitive(Graphics::TRIANGLE_STRIP);
        addSphereWithTexcoords(mMesh);
    }

    virtual void render(Graphics &g, float dt = 1.0f) override
    {
        mTexture.bind(0);
        g.draw(mMesh);
        mTexture.unbind();
    }

private:
    Texture mTexture;
    Mesh mMesh;
};

REGISTER_MODULE(MeshModule);

// ------------------------------ Render Tree

class RenderTree : public Relayer {
public:
    virtual ~RenderTree()
    {
        clear();
    }

    virtual void clear() {
        std::lock_guard<std::mutex> locker(mRenderChainLock);
        for(auto module: mModules) {
            module->cleanup();
        }
        mModules.clear();
    }

    virtual bool addModule(std::shared_ptr<RenderModule> module);

    void render(Graphics &g, float dt = 1.0f);

    template<class ModuleType>
    std::shared_ptr<ModuleType> createModule() {
        auto module = ModuleType::create();
        addModule(module);
        return module;
    }

    std::vector<std::shared_ptr<RenderModule>> modulesInTree() { return mModules; }

private:

	u_int32_t mCounter {0};
    std::vector<std::shared_ptr<RenderModule>> mModules;
    std::mutex mRenderChainLock;

    std::vector<std::shared_ptr<RenderModule>> mModulesPendingInit;
};

bool RenderTree::addModule(std::shared_ptr<RenderModule> module)
{
    std::lock_guard<std::mutex> locker(mRenderChainLock);
	module->setId(mCounter++);
	module->setRelayer(this);
    mModules.push_back(module);
    mModulesPendingInit.push_back(module);
}

void RenderTree::render(Graphics &g, float dt)
{
    std::lock_guard<std::mutex> locker(mRenderChainLock);
    for(auto module: mModulesPendingInit) {
        module->initInternal(g);
    }
    mModulesPendingInit.clear();

    std::vector<std::shared_ptr<RenderModule>> modulesToRemove;
    for(auto module: mModules) {
        if (module->done()) {
            module->cleanup();
            modulesToRemove.push_back(module);
        } else {
            module->renderInternal(g);
        }
    }
    for(auto module:modulesToRemove) {
        mModules.erase(std::find(mModules.begin(),mModules.end(), module));
    }
}

#include <functional>

class OSCAction {
public:
    OSCAction(std::string address, std::string types,
              std::function<bool(osc::Message &message, void *)> processingFunction,
              void *userData = nullptr)
    {
        mAddress = address;
        mTypes = types;
        mProcessingFunction = processingFunction;
        mUserData = userData;

        // Currently only supports a single wild card '*' as last character
        size_t addressWildcard = mAddress.find('*');
        if (addressWildcard != std::string::npos) {
            if (addressWildcard != mAddress.size()) {
                std::cout << "Warning, path truncated: " << mAddress;
                mAddress = mAddress.substr(0, addressWildcard);
                std::cout << " -> " << mAddress << std::endl;
            }
        }

        size_t typeWildcard = mTypes.find('*');
        if (typeWildcard != std::string::npos) {
            if (typeWildcard != mTypes.size()) {
                std::cout << "Warning, types truncated: " << mTypes;
                mTypes = mTypes.substr(0, typeWildcard);
                std::cout << " -> " << mTypes << std::endl;
            }
        }
    }

    virtual bool process(osc::Message &m) {
        std::string baseAddress = mAddress;
        size_t addressWildcard = mAddress.find('*');
        if (addressWildcard != std::string::npos) {
            baseAddress = mAddress.substr(0, addressWildcard + 1);
        }
        if(m.addressPattern().substr(0,baseAddress.size())  == baseAddress){
            std::string baseTags = mTypes;
            size_t wildCardIndex = mTypes.find('*');
            if (wildCardIndex != std::string::npos) {
                baseTags = mTypes.substr(0, wildCardIndex + 1);
            }
            if(m.typeTags().substr(0,baseTags.size())  == baseTags){
                return mProcessingFunction(m, mUserData);
            }
        }
        return false;
    }

private:
    std::string mAddress;
    std::function<bool (osc::Message &message, void *userData)> mProcessingFunction;
    std::string mTypes;
    void *mUserData;
};


/**
 * @brief The
 *
 * @ingroup allocore
 */
class RenderTreeHandler : public osc::MessageConsumer
{
public:
    RenderTreeHandler(RenderTree &tree) : mTree(&tree)
    {
        mOSCActions.push_back(
                    std::make_shared<OSCAction>("/createText", "si",
                                                [&] (osc::Message &m, void *userData) {
                        std::string text;
                        m >> text;
                        int id;
                        m >> id;
                        std::shared_ptr<TextRenderModule> module = static_cast<RenderTree *>(userData)->createModule<TextRenderModule>();
                        module->setText(text);
                        module->setId(id);
                        return true;
                    },
                    mTree
                    ));
        mOSCActions.push_back(
                    std::make_shared<OSCAction>("/createMesh", "i",
                                                [&] (osc::Message &m, void *userData) {
                        std::shared_ptr<MeshModule> module = static_cast<RenderTree *>(userData)->createModule<MeshModule>();
                        int id;
                        m >> id;
                        module->setId(id);
                        return true;
                    },
                    mTree
                    ));
    }

	std::vector<std::string> separateArguments(osc::Message &m, int argOffset = 0) {
		std::vector<std::string> arguments;
		for (size_t i = 0; i < m.typeTags().size() - argOffset ; i++) {
			std::string argument;
			if (m.typeTags()[i + argOffset] == 's') {
				m >> argument;
//				std::cout << "processed " << argument << std::endl;
			} else if (m.typeTags()[i + argOffset] == 'i') {
				int intArg;
				m >> intArg;
				argument = std::to_string(intArg);
//				std::cout << "processed " << argument << std::endl;
			} else if (m.typeTags()[i + argOffset] == 'f') {
				float floatArg;
				m >> floatArg;
				argument = std::to_string(floatArg);
//				std::cout << "processed " << argument << std::endl;
			}
			arguments.push_back(argument);
		}
		return arguments;
    }

    virtual bool consumeMessage(osc::Message& m, std::string rootOSCPath) override
    {
        std::string basePath = rootOSCPath;
        if (mOSCsubPath.size() > 0) {
            basePath += "/" + mOSCsubPath;
        }
		m.print();
        for(auto action : mOSCActions) {
            if (action->process(m)) {
                return true;
            }
        }
        if (m.addressPattern() == basePath + "/listModules" && m.typeTags() == "i") {
            int port;
            m >> port;
            osc::Send sender(port, m.senderAddress().c_str());
            std::cout << "Sending list to: " << m.senderAddress() << ":" << port << std::endl;
            std::vector<std::shared_ptr<RenderModule>> modules = mTree->modulesInTree();
            for (auto module: modules) {
                Vec3f position = module->getPosition();
                sender.beginMessage("/module");
                sender << module->getId() << module->getType();
                sender << position.x << position.y << position.z;
                sender.endMessage();
                sender.send();
            }
        } else if (m.addressPattern() == basePath + "/moduleCommand") {
            if (m.typeTags().size() > 1 && m.typeTags()[0] == 'i' && m.typeTags()[1] == 's') {
                int id;
				std::string command;
                m >> id >> command;
//                std::cout << "got command for id " << id << std::endl;
				std::vector<std::string> arguments = separateArguments(m, 2);
                for (auto module : mTree->modulesInTree()) {
                    if (module->getId() == id) {
                        module->executeCommand(command, arguments);
                        return true;
                    }
                    for (auto child: module->mChildren) {
                        if (child->getId() == id) {
                            child->executeCommand(command, arguments);
                            return true;
                        }
                    }
                }
                return true;
            }
        } else if (m.addressPattern() == basePath + "/create") {
			if (m.typeTags().size() == 2 && m.typeTags()[0] == 's' && m.typeTags()[1] == 'i') {
				std::string name;
				int id;
				m >> name >> id;
				for (auto moduleInfo : __renderModuleMap) {
					if (moduleInfo.first == name) {
						std::shared_ptr<RenderModule> module = moduleInfo.second();
						mTree->addModule(module);
						module->setId(id);
						module->relayCreation();
					}
				}
                return true;
            }
        } else {
			auto secondSlash = m.addressPattern().find("/", 1);
			if (secondSlash != std::string::npos) {
				std::string target = m.addressPattern().substr(1, secondSlash);
				std::string command = m.addressPattern().substr(secondSlash + 1, 128);
				uint32_t id = atoi(target.c_str());
				std::vector<std::string> arguments = separateArguments(m);
//				std::cout << "Processing command " << command << "for target " << target <<std::endl;
				for (auto module : mTree->modulesInTree()) {
					if (module->getId() == id) {
                        module->executeCommand(command, arguments);
						return true;
					}

					for (auto child: module->mChildren) {
						if (child->getId() == id) {
                            child->executeCommand(command, arguments);
							return true;
						}
					}
				}
			}
		}
        return false;
    }

    void setOSCsubPath(const std::string &oSCsubPath)
    {
        mOSCsubPath = oSCsubPath;
    }

protected:

private:
    RenderTree *mTree;
    std::string mOSCsubPath;
    std::vector<std::shared_ptr<OSCAction>> mOSCActions;
};

}

//--------------------------------------------------------------------------------------------------


#endif // RENDER_TREE_HPP
