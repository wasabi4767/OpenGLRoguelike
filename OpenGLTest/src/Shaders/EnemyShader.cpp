#include "EnemyShader.h"
#include <cstdio>
#include <cmath>

#if defined(_WIN32)
  #include <Windows.h>
#endif
#include <GL/gl.h>

// GLSL tokens
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

// GL2 function pointers 
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
    auto L = [](const char* n){ return glfwGetProcAddress(n); };
    struct Need{ const char* name; void** fp; } needs[] = {
        {"glCreateShader",(void**)&pglCreateShader},
        {"glShaderSource",(void**)&pglShaderSource},
        {"glCompileShader",(void**)&pglCompileShader},
        {"glGetShaderiv",(void**)&pglGetShaderiv},
        {"glGetShaderInfoLog",(void**)&pglGetShaderInfoLog},
        {"glDeleteShader",(void**)&pglDeleteShader},
        {"glCreateProgram",(void**)&pglCreateProgram},
        {"glAttachShader",(void**)&pglAttachShader},
        {"glLinkProgram",(void**)&pglLinkProgram},
        {"glGetProgramiv",(void**)&pglGetProgramiv},
        {"glGetProgramInfoLog",(void**)&pglGetProgramInfoLog},
        {"glGetUniformLocation",(void**)&pglGetUniformLocation},
        {"glUniform1f",(void**)&pglUniform1f},
        {"glUniform2f",(void**)&pglUniform2f},
        {"glUniform3f",(void**)&pglUniform3f},
        {"glUseProgram",(void**)&pglUseProgram},
    };
    bool ok=true;
    for(auto& it: needs){ *it.fp = L(it.name); if(!*it.fp){ std::fprintf(stderr,"[EnemyTri] missing: %s\n", it.name); ok=false; } }
    return ok;
}

// ---- shader program ----
namespace {
    GLuint gProg=0, gVert=0, gFrag=0;
    GLint  uCenter=-1, uSize=-1, uTime=-1, uDir=-1, uColA=-1, uColB=-1, uIntensity=-1;
    bool   gBuilt=false, gOK=false;

    const char* VERT =
        "void main(){\n"
        "  gl_Position = ftransform();\n"
        "}\n";


    const char* FRAG =
        "uniform vec2  uCenter;\n"
        "uniform float uSize;\n"
        "uniform float uTime;\n"
        "uniform vec2  uDir;\n"
        "uniform vec3  uColA;\n"
        "uniform vec3  uColB;\n"
        "uniform float uIntensity;\n"

        // ローカル基底（先端=+X）を作る
        "void basis(in vec2 dir, out vec2 R, out vec2 U){\n"
        "  vec2 r = normalize(dir);\n"
        "  R = r;\n"
        "  U = vec2(-r.y, r.x);\n"
        "}\n"

        // 2D 三角（凸多角形）の SDF：3つの半平面距離の最大値
        // 先端 v0=(H,0),右底 v1=(-B0, +B),左底 v2=(-B0, -B)
        // ポリゴンは CCW。各辺の“外向き”法線で半平面距離を作る。
        "float sdTriangle(vec2 p, float H, float B0, float B){\n"
        "  vec2 v0 = vec2( H, 0.0);\n"
        "  vec2 v1 = vec2(-B0, +B);\n"
        "  vec2 v2 = vec2(-B0, -B);\n"

        "  vec2 e0 = v1 - v0; vec2 n0 = normalize(vec2( e0.y, -e0.x)); // v0->v1\n"
        "  vec2 e1 = v2 - v1; vec2 n1 = normalize(vec2( e1.y, -e1.x)); // v1->v2\n"
        "  vec2 e2 = v0 - v2; vec2 n2 = normalize(vec2( e2.y, -e2.x)); // v2->v0\n"

        "  float d0 = dot(n0, p - v0);\n"
        "  float d1 = dot(n1, p - v1);\n"
        "  float d2 = dot(n2, p - v2);\n"

        "  // 内側: max(d0,d1,d2) <= 0、外側: >0。これをSDFとして使う。\n"
        "  return max(max(d0, d1), d2);\n"
        "}\n"

        "void main(){\n"
        "  // 画面→ローカル（先端+X）\n"
        "  vec2 gp = gl_FragCoord.xy - uCenter;\n"
        "  vec2 R, U; basis(uDir, R, U);\n"
        "  vec2 lp = vec2(dot(gp,R), dot(gp,U));\n"

        "  // 三角のジオメトリ（uSize を高さベースに拡大）\n"
        "  float H  = uSize * 1.6;  // 先端までの高さ\n"
        "  float B  = uSize * 0.9;  // 半底辺（幅/2）\n"
        "  float B0 = uSize * 0.7;  // 先端から底辺までのX距離（底辺の奥行き）\n"

        "  float d = sdTriangle(lp, H, B0, B);\n"

        "  // サイズに応じたAA（1〜3px）\n"
        "  float aa = clamp(uSize * 0.08, 1.0, 3.0);\n"
        "  float edge = 1.0 - smoothstep(0.0, aa, d);\n"

        "  // 内→外の色グラデ + 軽い脈動\n"
        "  float g = clamp((lp.x + B0) / (H + B0), 0.0, 1.0);\n" // 先端=1, 底辺=0 付近\n"
        "  float wave = 0.5 + 0.5*sin(uTime*4.0 + lp.y*0.06);\n"
        "  vec3 col = mix(uColA, uColB, g) * (0.85 + 0.15*wave) * uIntensity;\n"

        "  // ハロ（外側グロー）\n"
        "  float glow = smoothstep((H+B)*0.9, 0.0, length(lp));\n"
        "  float a = edge * 0.95 + glow * 0.10;\n"
        "  if(a <= 0.01) discard;\n"
        "  gl_FragColor = vec4(col, a);\n"
        "}\n";

    GLuint compile(GLenum type, const char* src){
        GLuint s = pglCreateShader(type);
        pglShaderSource(s,1,&src,nullptr);
        pglCompileShader(s);
        GLint ok=0; pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if(!ok){ char log[1024]; GLsizei n=0; pglGetShaderInfoLog(s,1024,&n,log);
            std::fprintf(stderr,"[EnemyTri] compile error:\n%.*s\n",(int)n,log);
            pglDeleteShader(s); return 0;
        }
        return s;
    }
}

namespace EnemyShader {
    bool EnsureBuilt(){
        if(gBuilt) return gOK;
        gBuilt = true;
        if(!loadGL2Funcs()){ std::fprintf(stderr,"[EnemyTri] GL2 funcs missing\n"); gOK=false; return false; }
        gVert = compile(GL_VERTEX_SHADER, VERT);
        gFrag = compile(GL_FRAGMENT_SHADER, FRAG);
        if(!gVert || !gFrag){ gOK=false; return false; }

        gProg = pglCreateProgram();
        pglAttachShader(gProg, gVert);
        pglAttachShader(gProg, gFrag);
        pglLinkProgram(gProg);
        GLint ok=0; pglGetProgramiv(gProg, GL_LINK_STATUS, &ok);
        if(!ok){ char log[1024]; GLsizei n=0; pglGetProgramInfoLog(gProg,1024,&n,log);
            std::fprintf(stderr,"[EnemyTri] link error:\n%.*s\n",(int)n,log);
            gOK=false; return false; }

        uCenter    = pglGetUniformLocation(gProg, "uCenter");
        uSize      = pglGetUniformLocation(gProg, "uSize");
        uTime      = pglGetUniformLocation(gProg, "uTime");
        uDir       = pglGetUniformLocation(gProg, "uDir");
        uColA      = pglGetUniformLocation(gProg, "uColA");
        uColB      = pglGetUniformLocation(gProg, "uColB");
        uIntensity = pglGetUniformLocation(gProg, "uIntensity");
        gOK = true; return true;
    }

    bool IsSupported(){ return EnsureBuilt(); }
    void Use(){ if(gOK) pglUseProgram(gProg); }
    void Stop(){ if(gOK) pglUseProgram(0); }

    void SetUniforms(float cx, float cy,
                     float size,
                     float time,
                     float dirX, float dirY,
                     float cax, float cay, float caz,
                     float cbx, float cby, float cbz,
                     float intensity){
        if(!gOK) return;
        float len = std::sqrt(dirX*dirX + dirY*dirY);
        if (len < 1e-6f) { dirX = 1.0f; dirY = 0.0f; }
        else { dirX /= len; dirY /= len; }

        pglUniform2f(uCenter, cx, cy);
        pglUniform1f(uSize, size);
        pglUniform1f(uTime, time);
        pglUniform2f(uDir, dirX, dirY);
        pglUniform3f(uColA, cax, cay, caz);
        pglUniform3f(uColB, cbx, cby, cbz);
        pglUniform1f(uIntensity, intensity);
    }
}
