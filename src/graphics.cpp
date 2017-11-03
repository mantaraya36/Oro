#include "alloutil/al_OmniApp.hpp"
// #include "alloutil/al_OmniStereoGraphicsRenderer.hpp"
#include "al_OmniStereoGraphicsRenderer2.hpp"

using namespace al;
using namespace std;




/*
Allocore Example: Simple App

Description:
This example demonstrates how to use the App class to create a 3D space with
audio and graphics rendering callbacks.

The App class (and any subclass thereof) has some default keyboard and mouse
controls assigned automatically. These are:

	Keyboard		Action
	'w'				move forward
	'x'				move back
	'a'				move left
	'd'				move right
	'e'				move up
	'c'				move down

	'q'				roll counter-clockwise
	'z'				roll clockwise
	left arrow		turn left
	right arrow		turn right
	up arrow		turn up
	down arrow		turn down

	Mouse
	drag left		turn left
	drag right		turn right
	drag up			turn up
	drag down		turn down

For the camera, the coordinate conventions are:

	-x is left
	+x is right
	-y is down
	+y is up
	-z is forward
	+z is backward

Author:
Lance Putnam, 6/2011, putnam.lance@gmail.com
*/

#include <iostream>
#include <memory>

#include "Cuttlebone/Cuttlebone.hpp"

#include "allocore/graphics/al_Shapes.hpp"
#include "allocore/graphics/al_Image.hpp"
#include "allocore/math/al_Random.hpp"
#include "allocore/graphics/al_Texture.hpp"
#include "allocore/ui/al_Parameter.hpp"
#include "alloGLV/al_ParameterGUI.hpp"
#include "alloutil/al_OmniApp.hpp"

#include "Gamma/Noise.h"
#include "Gamma/Filter.h"
#include "Gamma/SoundFile.h"
#include "Gamma/Oscillator.h"

#include "common.hpp"

//#define SURROUND
using namespace al;
using namespace std;
// #define BUILDING_FOR_ALLOSPHERE

std::string wbVertexCode() {
    return R"(
#version 120
void main() {
  gl_TexCoord[0] = gl_MultiTexCoord0;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)";
}
std::string wbFragmentCode() {
    return R"(
#version 120
uniform sampler2D texture_warp;
// uniform sampler2D texture_blend;
void main() {
    // float blend = texture2D(texture_blend, gl_TexCoord[0].st).r;
    vec3 dir = texture2D(texture_warp, gl_TexCoord[0].st).rgb;
    dir = normalize(dir);
    dir += 1.0;
    dir *= 0.5;
    gl_FragColor = vec4(dir, 1.0);
}
)";
}


std::string effectVertexCode() {
  return R"(
#version 120
void main() {
  gl_TexCoord[0] = gl_MultiTexCoord0;
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
)";
}

std::string effectFragmentCode() {
  return R"(
#version 120

//
// Description : Array and textureless GLSL 2D/3D/4D simplex
//               noise functions.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : ijm
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
//

vec4 mod289(vec4 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0; }

float mod289(float x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0; }

vec4 permute(vec4 x) {
     return mod289(((x*34.0)+1.0)*x);
}

float permute(float x) {
     return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
  return 1.79284291400159 - 0.85373472095314 * r;
}

float taylorInvSqrt(float r)
{
  return 1.79284291400159 - 0.85373472095314 * r;
}

vec4 grad4(float j, vec4 ip)
{
    const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
    vec4 p,s;

    p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
    p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
    s = vec4(lessThan(p, vec4(0.0)));
    p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www;

    return p;
}

// (sqrt(5) - 1)/4 = F4, used once below
#define F4 0.309016994374947451

float snoise(vec4 v)
{
  const vec4  C = vec4( 0.138196601125011,  // (5 - sqrt(5))/20  G4
                        0.276393202250021,  // 2 * G4
                        0.414589803375032,  // 3 * G4
                       -0.447213595499958); // -1 + 4 * G4

  // First corner
  vec4 i  = floor(v + dot(v, vec4(F4)) );
  vec4 x0 = v -   i + dot(i, C.xxxx);

  // Other corners

  // Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  vec4 i0;
  vec3 isX = step( x0.yzw, x0.xxx );
  vec3 isYZ = step( x0.zww, x0.yyz );
  //  i0.x = dot( isX, vec3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;
  //  i0.y += dot( isYZ.xy, vec2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;
  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  vec4 i3 = clamp( i0, 0.0, 1.0 );
  vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C.xxxx
  //  x1 = x0 - i1  + 1.0 * C.xxxx
  //  x2 = x0 - i2  + 2.0 * C.xxxx
  //  x3 = x0 - i3  + 3.0 * C.xxxx
  //  x4 = x0 - 1.0 + 4.0 * C.xxxx
  vec4 x1 = x0 - i1 + C.xxxx;
  vec4 x2 = x0 - i2 + C.yyyy;
  vec4 x3 = x0 - i3 + C.zzzz;
  vec4 x4 = x0 + C.wwww;

  // Permutations
  i = mod289(i);
  float j0 = permute( permute( permute( permute(i.w) + i.z) + i.y) + i.x);
  vec4 j1 = permute( permute( permute( permute (
             i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
           + i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));

  // Gradients: 7x7x6 points over a cube, mapped onto a 4-cross polytope
  // 7*7*6 = 294, which is close to the ring size 17*17 = 289.
  vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  vec4 p0 = grad4(j0,   ip);
  vec4 p1 = grad4(j1.x, ip);
  vec4 p2 = grad4(j1.y, ip);
  vec4 p3 = grad4(j1.z, ip);
  vec4 p4 = grad4(j1.w, ip);

  // Normalise gradients
  vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= taylorInvSqrt(dot(p4,p4));

  // Mix contributions from the five corners
  vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}

uniform sampler2D texture0;
uniform sampler2D wb_tex;
uniform float time;
uniform float radius;

void main() {
  vec3 dir = texture2D(wb_tex, gl_TexCoord[0].st).rgb;
  dir *= 2.0;
  dir -= 1.0; // [0:1] to [-1:+1]

  vec4 st1 = vec4(radius * dir, time); // dir in space + time
  float n1 = snoise(st1);

  vec4 st2 = 2 * vec4(radius * dir, time);
  float n2 = snoise(st2);

  vec4 st3 = 4 * vec4(radius * dir, time);
  float n3 = snoise(st3);

  float n = (n1 + 0.5 * n2 + 0.25 * n3) / (1.0 + 0.5 + 0.25);
  n += 1.0;
  n *= 0.5; // [-1:+1] to [0:1]

  vec4 textureColor = texture2D(texture0, gl_TexCoord[0].st);
  float multiplier = 0.9 + 0.1 * n; // 0.25 + 0.75 * n;
  gl_FragColor = vec4(multiplier * textureColor.rgb, textureColor.a);
}
)";
}

struct Renderer : public OmniStereoGraphicsRenderer2 {
public:

  bool do_aftereffect = true;

    SharedState mState;
    SharedPainter mPainter;

    Texture capture_tex[2];
    Texture temp_capture_tex;
    int capture_idx = 0;
    RBO rbo;
    FBO fbo;
    ShaderProgram effectShader;

    // for recording warp and blend
    RBO wb_rbo;
    FBO wb_fbo;
    Texture wb_tex;
    ShaderProgram wb_shader;

    SharedState& state() {return mState;}

    cuttlebone::Taker<SharedState> mTaker;
//    float mMouseX, mMouseY;
//    float mSpeedX, mSpeedY;
    // From control interface
//    float mPosX {0};
//    float mPosY {0};
//    float mDeltaX {0};
//    float mDeltaY {0};

	ShaderProgram mShader;

//    // Audio
//	Granulator granX, granY, granZ;
//	Granulator background1;
//	Granulator background2;
//	Granulator background3;
//    gam::SineR<float> fluctuation1, fluctuation2;

//    // Parameters
//	Parameter gainBackground1 {"background1", "", 0.2f, "", 0.0f, 1.0f};
//	Parameter gainBackground2 {"background2", "", 0.2f, "", 0.0f, 1.0f};
//	Parameter gainBackground3 {"background3", "", 0.2f, "", 0.0f, 1.0f};

//	ParameterGUI mParameterGUI;

	// This constructor is where we initialize the application
	Renderer(): mPainter(&mState, &shader(), GRAPHICS_IN_PORT, GRAPHICS_SLAVE_PORT)

	{
//		AudioDevice::printAll();

//		mParameterGUI.setParentApp(this);
//		mParameterGUI << new glv::Label("Parameter GUI example");
//		mParameterGUI << gainBackground1 << gainBackground2 << gainBackground3;

//        mPainter.setTreeMaster();
//        std::shared_ptr<TextRenderModule> module = mPainter.mRenderTree.createModule<TextRenderModule>();
//        module->setText("Hello");
//        module->setPosition(Vec3f(0, 0, -1));
		std::cout << "Constructor done" << std::endl;

	}

	virtual bool onCreate() override {
//		mShader.compile(fogVert, fogFrag);
        OmniStereoGraphicsRenderer2::onCreate();

        nav().pos().set(0,3,10);
//		nav().quat().fromAxisAngle(0.*M_2PI, 0,1,0);
        // omni().clearColor() = Color(0.02, 0.02, 0.04, 0.0);
        omni().clearColor() = Color(0, 0, 0, 0);

        return true;
	}

    ~Renderer() {
        mTaker.stop();
    }

    void setup() {

        mPainter.onInit();

        auto& s = shader();
        s.begin();
        // the shader will look for texture0 uniform's data
        // at binding point '0' >> we will bind the texture at '0'
        s.uniform("texture0", 0);
        // how much we mix the texture
        s.uniform("texture", 1.0);
        s.uniform("lighting", 0.9);
        s.end();

        Shader vert, frag;
        vert.source(effectVertexCode(), Shader::VERTEX).compile();
        vert.printLog();
        frag.source(effectFragmentCode(), Shader::FRAGMENT).compile();
        frag.printLog();
        effectShader.attach(vert).attach(frag).link();
        effectShader.printLog();
        effectShader.begin();
        effectShader.uniform("texture0", 0);
        effectShader.uniform("wb_tex", 1);
        effectShader.end();

        Shader wb_vert, wb_frag;
        wb_vert.source(wbVertexCode(), Shader::VERTEX).compile();
        wb_vert.printLog();
        wb_frag.source(wbFragmentCode(), Shader::FRAGMENT).compile();
        wb_frag.printLog();
        wb_shader.attach(wb_vert).attach(wb_frag).link();
        wb_shader.printLog();
        wb_shader.begin();
        wb_shader.uniform("texture_warp", 0);
        // wb_shader.uniform("texture_blend", 1);
        wb_shader.end();
    }

    virtual void start() {
        mTaker.start();                        // non-blocking
        OmniStereoGraphicsRenderer2::start();  // blocks
    }

    virtual void onAnimate(double dt) override {
        int got = mTaker.get(state());
//        if (got > 0) {
//            std::cout << "got " << got << std::endl;
//        }
        
        static bool first_frame = true;
        if (first_frame) {
          setup();
          first_frame = false;
        }
        // Update wave equation
        for(int j=0; j<Ny; ++j){
            for(int i=0; i<Nx; ++i){
                int idx = j*Nx + i;
                mPainter.waterMesh.vertices()[idx].z = state().wave[indexAt(i,j, state().zcurr)] / state().decay;
            }
        }

        mPainter.waterMesh.generateNormals();
    }

    virtual void onDraw(Graphics& g) override {
        mPainter.onDraw(g);
    }

    void afterEffect(Graphics& g) override {

        if (!do_aftereffect) return;

        // resize if needed
        capture_tex[0].resize(width(), height());
        capture_tex[1].resize(width(), height());
        temp_capture_tex.resize(width(), height());
        rbo.resize(width(), height());

        wb_tex.resize(width(), height());
        wb_rbo.resize(width(), height());
        wb_fbo.attachRBO(rbo, FBO::DEPTH_ATTACHMENT);
        wb_fbo.attachTexture2D(wb_tex.id(), FBO::COLOR_ATTACHMENT0);

        // prepare wb texture
        wb_fbo.begin();
        g.clearColor(1, 0, 0, 1);
        g.clear(Graphics::COLOR_BUFFER_BIT | Graphics::DEPTH_BUFFER_BIT);
        g.blending(false);
        wb_shader.begin();
        auto& o = omni();
        for (int i = 0; i < o.numProjections(); i += 1) {
            OmniStereo::Projection& p = o.projection(i);
            Viewport& v = p.viewport();
            Viewport viewport(v.l * width(), v.b * height(), v.w * width(), v.h * height());
            g.viewport(viewport); // also scissors

            g.clearColor(1, 1, 1, 1);
            g.clear(Graphics::COLOR_BUFFER_BIT | Graphics::DEPTH_BUFFER_BIT);

            g.projection(Matrix4d::identity());
            g.modelView(Matrix4d::identity());

            p.warp().bind(0);
            // p.blend().bind(1);

            Mesh m {Graphics::TRIANGLE_STRIP};
            m.vertex(-1, -1, 0);
            m.vertex( 1, -1, 0);
            m.vertex(-1,  1, 0);
            m.vertex( 1,  1, 0);

            m.texCoord(0, 0);
            m.texCoord(1, 0);
            m.texCoord(0, 1);
            m.texCoord(1, 1);

            g.draw(m);

            p.warp().unbind(0);
            // p.blend().unbind(1);
        }
        wb_shader.end();
        wb_fbo.end();
        g.viewport(0, 0, width(), height()); // put back viewport

        // copy the result from omni rendering
        temp_capture_tex.copyFrameBuffer();

        // add ripple to capture
        // use warp and blend texture made above
        fbo.attachRBO(rbo, FBO::DEPTH_ATTACHMENT);
        fbo.attachTexture2D(capture_tex[capture_idx].id(), FBO::COLOR_ATTACHMENT0);
        fbo.begin();
        g.blending(false);
        effectShader.begin();
        effectShader.uniform("radius", 2.5);
        effectShader.uniform("time", state().casasPhase * 0.1f);
        wb_tex.bind(1);
        temp_capture_tex.quadViewport(g);
        wb_tex.unbind(1);
        effectShader.end();
        fbo.end();

        // clear all        
        g.clearColor(0, 0, 0, 1);
        g.clear(Graphics::COLOR_BUFFER_BIT | Graphics::DEPTH_BUFFER_BIT);
        g.depthTesting(false);

        // opaque render previous frame
        g.blending(false);
        // capture_tex[1 - capture_idx].quadViewport(g, Color(1, 1, 1, 1));

        // blend current new frame
        // g.blending(true);
        // g.blendMode(Graphics::ONE, Graphics::ONE_MINUS_SRC_ALPHA, Graphics::FUNC_ADD);
        // g.blendTrans();
        capture_tex[capture_idx].quadViewport(g, Color(1, 1, 1, 1));

        // copy again composited result
        // capture_tex[capture_idx].copyFrameBuffer();

        // debug wb texture
        // g.blending(false);
        // wb_tex.quadViewport(g, Color(1, 1, 1, 1), 1, 1, -0.5, -0.5);
        // wb_tex.quadViewport(g, Color(1, 1, 1, 1), 2, 2, -1, -1);

        // and swap capture texture
        capture_idx = 1- capture_idx;
    }

//    virtual void onDrawOmni(OmniStereo &om) override {
//        mPainter.onDraw(om.graphics() );
//	}

    virtual std::string vertexCode() override {
        return vertexShader;
    }
    virtual std::string fragmentCode() override {
        return fragmentShader;
    }
};


int main() {
  Renderer().start();
}
