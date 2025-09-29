#include "PlayerFlareShader.h"
#include <cstdio>
#include <cmath>

#ifndef GLchar
typedef char GLchar;
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

// GL2関数ポインタ
typedef GLuint (APIENTRY * PFNGLCREATESHADERPROC) (GLenum);
typedef void   (APIENTRY * PFNGLSHADERSOURCEPROC) (GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (APIENTRY * PFNGLCOMPILESHADERPROC)(GLuint);
typedef void   (APIENTRY * PFNGLGETSHADERIVPROC)  (GLuint, GLenum, GLint*);
typedef void   (APIENTRY * PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY * PFNGLDELETESHADERPROC) (GLuint);

typedef GLuint (APIENTRY * PFNGLCREATEPROGRAMPROC)(void);
typedef void   (APIENTRY * PFNGLATTACHSHADERPROC) (GLuint, GLuint);
typedef void   (APIENTRY * PFNGLLINKPROGRAMPROC)  (GLuint);
typedef void   (APIENTRY * PFNGLGETPROGRAMIVPROC) (GLuint, GLenum, GLint*);
typedef void   (APIENTRY * PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);

typedef GLint  (APIENTRY * PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);
typedef void   (APIENTRY * PFNGLUNIFORM1FPROC) (GLint, GLfloat);
typedef void   (APIENTRY * PFNGLUNIFORM2FPROC) (GLint, GLfloat, GLfloat);
typedef void   (APIENTRY * PFNGLUNIFORM3FPROC) (GLint, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY * PFNGLUNIFORM4FPROC) (GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY * PFNGLUSEPROGRAMPROC)(GLuint);

static PFNGLCREATESHADERPROC        pglCreateShader        = nullptr;
static PFNGLSHADERSOURCEPROC        pglShaderSource        = nullptr;
static PFNGLCOMPILESHADERPROC       pglCompileShader       = nullptr;
static PFNGLGETSHADERIVPROC         pglGetShaderiv         = nullptr;
static PFNGLGETSHADERINFOLOGPROC    pglGetShaderInfoLog    = nullptr;
static PFNGLDELETESHADERPROC        pglDeleteShader        = nullptr;

static PFNGLCREATEPROGRAMPROC       pglCreateProgram       = nullptr;
static PFNGLATTACHSHADERPROC        pglAttachShader        = nullptr;
static PFNGLLINKPROGRAMPROC         pglLinkProgram         = nullptr;
static PFNGLGETPROGRAMIVPROC        pglGetProgramiv        = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC   pglGetProgramInfoLog   = nullptr;

static PFNGLGETUNIFORMLOCATIONPROC  pglGetUniformLocation  = nullptr;
static PFNGLUNIFORM1FPROC           pglUniform1f           = nullptr;
static PFNGLUNIFORM2FPROC           pglUniform2f           = nullptr;
static PFNGLUNIFORM3FPROC           pglUniform3f           = nullptr;
static PFNGLUNIFORM4FPROC           pglUniform4f           = nullptr;
static PFNGLUSEPROGRAMPROC          pglUseProgram          = nullptr;

static bool loadGL2Funcs() {
    auto L = [](const char* n){ return glfwGetProcAddress(n); };
    pglCreateShader      = (PFNGLCREATESHADERPROC)      L("glCreateShader");
    pglShaderSource      = (PFNGLSHADERSOURCEPROC)      L("glShaderSource");
    pglCompileShader     = (PFNGLCOMPILESHADERPROC)     L("glCompileShader");
    pglGetShaderiv       = (PFNGLGETSHADERIVPROC)       L("glGetShaderiv");
    pglGetShaderInfoLog  = (PFNGLGETSHADERINFOLOGPROC)  L("glGetShaderInfoLog");
    pglDeleteShader      = (PFNGLDELETESHADERPROC)      L("glDeleteShader");
    pglCreateProgram     = (PFNGLCREATEPROGRAMPROC)     L("glCreateProgram");
    pglAttachShader      = (PFNGLATTACHSHADERPROC)      L("glAttachShader");
    pglLinkProgram       = (PFNGLLINKPROGRAMPROC)       L("glLinkProgram");
    pglGetProgramiv      = (PFNGLGETPROGRAMIVPROC)      L("glGetProgramiv");
    pglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) L("glGetProgramInfoLog");
    pglGetUniformLocation= (PFNGLGETUNIFORMLOCATIONPROC)L("glGetUniformLocation");
    pglUniform1f         = (PFNGLUNIFORM1FPROC)         L("glUniform1f");
    pglUniform2f         = (PFNGLUNIFORM2FPROC)         L("glUniform2f");
    pglUniform3f         = (PFNGLUNIFORM3FPROC)         L("glUniform3f");
    pglUniform4f         = (PFNGLUNIFORM4FPROC)         L("glUniform4f");
    pglUseProgram        = (PFNGLUSEPROGRAMPROC)        L("glUseProgram");

    return pglCreateShader && pglShaderSource && pglCompileShader &&
           pglGetShaderiv && pglGetShaderInfoLog && pglDeleteShader &&
           pglCreateProgram && pglAttachShader && pglLinkProgram &&
           pglGetProgramiv && pglGetProgramInfoLog &&
           pglGetUniformLocation && pglUniform1f && pglUniform2f &&
           pglUniform3f && pglUniform4f && pglUseProgram;
}

// シェーダ本体
namespace {
    GLuint gProg = 0, gVert = 0, gFrag = 0;
    GLint  uCenter=-1, uInnerR=-1, uOuterR=-1, uTime=-1, uColA=-1, uColB=-1, uIntensity=-1;
    GLint  uHitAmt=-1, uInvOn=-1;

    bool   gBuilt=false, gOK=false;

    const char* VERT =
        "void main(){\n"
        "  gl_Position = ftransform();\n"
        "}\n";

    // 青↔緑のグラデ＋柔らかいハロ＋軽いリング波＆擬似フレネル
    const char* FRAG =
     "uniform vec2  uCenter;\n"
     "uniform float uInnerR;\n"
     "uniform float uOuterR;\n"
     "uniform float uTime;\n"
     "uniform vec3  uColA;\n"
     "uniform vec3  uColB;\n"
     "uniform float uIntensity;\n"
     "uniform float uHitAmt;\n"   
     "uniform float uInvOn;\n"  
     "void main(){\n"
     "  vec2  p = gl_FragCoord.xy;\n"
     "  float d = distance(p, uCenter);\n"
     "  float t = uTime;\n"
     "  float g = clamp((d - uInnerR) / max(1e-4, (uOuterR - uInnerR)), 0.0, 1.0);\n"
     "  vec3 baseCol = mix(uColA, uColB, smoothstep(0.0, 1.0, g));\n"
     "  float core = 1.0 - smoothstep(uInnerR-1.5, uInnerR+1.5, d);\n"
     "  float halo = 1.0 - smoothstep(uInnerR, uOuterR, d);\n"
     "  float wave = 0.5 + 0.5 * sin(d*0.10 - t*3.5);\n"
     "  float fres = pow(1.0 - g, 0.6);\n"
     "  float a = clamp(core*0.95 + halo*(0.45*wave + 0.25*fres), 0.0, 1.0);\n"
     " vec3 col = baseCol * (0.70 + 0.30*wave) * uIntensity;\n"
     " // 無敵中も赤くする\n"
     " float redMix = max(clamp(uHitAmt,0.0,1.0), clamp(uInvOn,0.0,1.0));\n"
     " col = mix(vec3(1.0, 0.2, 0.2), col, 1.0 - redMix);\n"
     " // 無敵点滅（アルファは点滅のまま）\n"
     " float blink = (fract(t*8.0) > 0.5) ? 0.25 : 1.0;\n"
     " a *= mix(1.0, blink, clamp(uInvOn,0.0,1.0));\n"
     "  a *= mix(1.0, blink, clamp(uInvOn,0.0,1.0));\n"
     "  gl_FragColor = vec4(col, a);\n"
     "}\n";


    GLuint compile(GLenum type, const char* src){
        GLuint s = pglCreateShader(type);
        pglShaderSource(s, 1, &src, nullptr);
        pglCompileShader(s);
        GLint ok=0; pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if(!ok){
            char log[1024]; GLsizei n=0; pglGetShaderInfoLog(s,1024,&n,log);
            std::fprintf(stderr,"[PlayerFlare] shader compile error:\n%.*s\n",(int)n,log);
            pglDeleteShader(s); return 0;
        }
        return s;
    }
}

namespace PlayerFlareShader {
    bool EnsureBuilt(){
        if(gBuilt) return gOK;
        gBuilt = true;

        if(!loadGL2Funcs()){
            std::fprintf(stderr,"[PlayerFlare] GL2 functions missing.\n");
            gOK=false; return false;
        }
        gVert = compile(GL_VERTEX_SHADER,   VERT);
        gFrag = compile(GL_FRAGMENT_SHADER, FRAG);
        if(!gVert || !gFrag){ gOK=false; return false; }

        gProg = pglCreateProgram();
        pglAttachShader(gProg, gVert);
        pglAttachShader(gProg, gFrag);
        pglLinkProgram(gProg);
        GLint ok=0; pglGetProgramiv(gProg, GL_LINK_STATUS, &ok);
        if(!ok){
            char log[1024]; GLsizei n=0; pglGetProgramInfoLog(gProg,1024,&n,log);
            std::fprintf(stderr,"[PlayerFlare] program link error:\n%.*s\n",(int)n,log);
            gOK=false; return false;
        }

        uCenter    = pglGetUniformLocation(gProg, "uCenter");
        uInnerR    = pglGetUniformLocation(gProg, "uInnerR");
        uOuterR    = pglGetUniformLocation(gProg, "uOuterR");
        uTime      = pglGetUniformLocation(gProg, "uTime");
        uColA      = pglGetUniformLocation(gProg, "uColA");
        uColB      = pglGetUniformLocation(gProg, "uColB");
        uIntensity = pglGetUniformLocation(gProg, "uIntensity");
        uHitAmt    = pglGetUniformLocation(gProg, "uHitAmt");
        uInvOn     = pglGetUniformLocation(gProg, "uInvOn");
        gOK = true;
        return true;
    }

    bool IsSupported(){ return EnsureBuilt(); }
    void Use(){ if(gOK) pglUseProgram(gProg); }
    void Stop(){ if(gOK) pglUseProgram(0); }

    void SetUniforms(float cx, float cy,
                     float innerR, float outerR,
                     float time,
                     float cax, float cay, float caz,
                     float cbx, float cby, float cbz,
                     float intensity,
                     float hitAmt,
                     float invOn){
        if(!gOK) return;
        pglUniform2f(uCenter, cx, cy);
        pglUniform1f(uInnerR, innerR);
        pglUniform1f(uOuterR, outerR);
        pglUniform1f(uTime,   time);
        pglUniform3f(uColA,   cax, cay, caz);
        pglUniform3f(uColB,   cbx, cby, cbz);
        pglUniform1f(uIntensity, intensity);
        pglUniform1f(uHitAmt, hitAmt);
        pglUniform1f(uInvOn,  invOn);
    }
}
