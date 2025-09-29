#include "NeonOrbShader.h"
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

// ---- OpenGL 2.0+ 関数を動的ロード（glfwGetProcAddress） ----------------
#ifndef APIENTRY
#define APIENTRY
#endif

// 必要な関数ポインタ typedef
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
typedef void   (APIENTRY * PFNGLUSEPROGRAMPROC)(GLuint);

// 関数ポインタ実体
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
static PFNGLUSEPROGRAMPROC          pglUseProgram          = nullptr;

static bool loadGL2Funcs() {
    // GLFWのローダで取得（コンテキスト作成後に呼ぶ必要あり）
    auto L = [](const char* name){ return glfwGetProcAddress(name); };

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
    pglUseProgram        = (PFNGLUSEPROGRAMPROC)        L("glUseProgram");

    // どれか欠けていたらNG（フォールバックへ）
    return pglCreateShader && pglShaderSource && pglCompileShader &&
           pglGetShaderiv && pglGetShaderInfoLog && pglDeleteShader &&
           pglCreateProgram && pglAttachShader && pglLinkProgram &&
           pglGetProgramiv && pglGetProgramInfoLog &&
           pglGetUniformLocation && pglUniform1f && pglUniform2f &&
           pglUniform3f && pglUseProgram;
}

// ---- シェーダ本体 ---------------------------------------------------------
namespace {
    GLuint gProg = 0, gVert = 0, gFrag = 0;
    GLint  uCenter = -1, uInnerR = -1, uOuterR = -1, uTime = -1, uColor = -1;
    bool   gBuilt = false, gOK = false;

    const char* VERT =
        "void main(){\n"
        "  gl_Position = ftransform();\n"
        "}\n";

    const char* FRAG =
        "uniform vec2  uCenter;\n"
        "uniform float uInnerR;\n"
        "uniform float uOuterR;\n"
        "uniform float uTime;\n"
        "uniform vec3  uColor;\n"
        "void main(){\n"
        "  vec2  p = gl_FragCoord.xy;\n"
        "  float d = distance(p, uCenter);\n"
        "  float core = 1.0 - smoothstep(uInnerR-1.5, uInnerR+1.5, d);\n"
        "  float halo = 1.0 - smoothstep(uInnerR, uOuterR, d);\n"
        "  float wave = 0.5 + 0.5 * sin(d*0.13 - uTime*4.5);\n"
        "  float a = clamp(core*0.9 + halo*0.6*wave, 0.0, 1.0);\n"
        "  vec3 c = uColor * (0.65 + 0.35*wave);\n"
        "  gl_FragColor = vec4(c, a);\n"
        "}\n";

    GLuint compile(GLenum type, const char* src){
        GLuint s = pglCreateShader(type);
        pglShaderSource(s, 1, &src, nullptr);
        pglCompileShader(s);
        GLint ok=0; pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if(!ok){
            char log[1024]; GLsizei n=0; pglGetShaderInfoLog(s,1024,&n,log);
            std::fprintf(stderr,"[NeonOrb] shader compile error:\n%.*s\n",(int)n,log);
            pglDeleteShader(s); return 0;
        }
        return s;
    }
}

namespace NeonOrbShader {
    bool EnsureBuilt(){
        if(gBuilt) return gOK;
        gBuilt = true;

        // OpenGL 2.0+ の関数をロード（GLFWコンテキスト作成後に呼ばれる前提）
        if(!loadGL2Funcs()){
            std::fprintf(stderr, "[NeonOrb] GL2 functions missing. Fallback to fixed pipeline.\n");
            gOK = false; return false;
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
            std::fprintf(stderr,"[NeonOrb] program link error:\n%.*s\n",(int)n,log);
            gOK=false; return false;
        }

        uCenter = pglGetUniformLocation(gProg, "uCenter");
        uInnerR = pglGetUniformLocation(gProg, "uInnerR");
        uOuterR = pglGetUniformLocation(gProg, "uOuterR");
        uTime   = pglGetUniformLocation(gProg, "uTime");
        uColor  = pglGetUniformLocation(gProg, "uColor");
        gOK = true;
        return true;
    }

    bool IsSupported(){ return EnsureBuilt(); }

    void Use(){ if(gOK) pglUseProgram(gProg); }
    void Stop(){ if(gOK) pglUseProgram(0); }

    void SetUniforms(float cx, float cy, float innerR, float outerR,
                     float time, float r, float g, float b){
        if(!gOK) return;
        pglUniform2f(uCenter, cx, cy);
        pglUniform1f(uInnerR, innerR);
        pglUniform1f(uOuterR, outerR);
        pglUniform1f(uTime,   time);
        pglUniform3f(uColor,  r, g, b);
    }
}
