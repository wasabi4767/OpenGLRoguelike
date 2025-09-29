#include "PlayerSmokeShader.h"
#include <cstdio>
#include <cmath>

// ★ Windows の場合は先に <Windows.h> を
#if defined(_WIN32)
  #include <Windows.h>
#endif
// ★ 基本の GL 型・定数は正規のヘッダから取得（ABI不一致を避ける）
#include <GL/gl.h>

// ---- 必要最小の GLSL トークン（不足環境向け） ----
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

// ---- GL2 関数ポインタ宣言（glfwGetProcAddress で動的ロード） ----
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

// 関数ポインタを厳密チェックでロード（どれが欠けたかも表示）
static bool loadGL2Funcs() {
    auto L = [](const char* n){ return glfwGetProcAddress(n); };

    struct Need { const char* name; void** fp; } needs[] = {
        {"glCreateShader", (void**)&pglCreateShader},
        {"glShaderSource", (void**)&pglShaderSource},
        {"glCompileShader",(void**)&pglCompileShader},
        {"glGetShaderiv",  (void**)&pglGetShaderiv},
        {"glGetShaderInfoLog",(void**)&pglGetShaderInfoLog},
        {"glDeleteShader", (void**)&pglDeleteShader},
        {"glCreateProgram",(void**)&pglCreateProgram},
        {"glAttachShader", (void**)&pglAttachShader},
        {"glLinkProgram",  (void**)&pglLinkProgram},
        {"glGetProgramiv", (void**)&pglGetProgramiv},
        {"glGetProgramInfoLog",(void**)&pglGetProgramInfoLog},
        {"glGetUniformLocation",(void**)&pglGetUniformLocation},
        {"glUniform1f",    (void**)&pglUniform1f},
        {"glUniform2f",    (void**)&pglUniform2f},
        {"glUniform3f",    (void**)&pglUniform3f},
        {"glUseProgram",   (void**)&pglUseProgram},
    };

    bool ok = true;
    for (auto& it : needs) {
        *it.fp = L(it.name);
        if (!*it.fp) {
            std::fprintf(stderr, "[GL2 Load] missing: %s\n", it.name);
            ok = false;
        }
    }
    return ok;
}

// ---- シェーダ本体 ----
namespace {
    GLuint gProg=0, gVert=0, gFrag=0;
    GLint  uCenter=-1, uInnerR=-1, uOuterR=-1, uTime=-1, uColor=-1, uIntensity=-1, uNoiseScale=-1, uSwirl=-1;
    bool   gBuilt=false, gOK=false;

    const char* VERT =
        "void main(){\n"
        "  gl_Position = ftransform();\n"
        "}\n";

    // FBMノイズで煙っぽいアルファ、緩い渦（swirl）でまとわりつく動き
    const char* FRAG =
        "uniform vec2  uCenter;\n"
        "uniform float uInnerR;\n"
        "uniform float uOuterR;\n"
        "uniform float uTime;\n"
        "uniform vec3  uColor;\n"
        "uniform float uIntensity;\n"
        "uniform float uNoiseScale;\n"
        "uniform float uSwirl;\n"

        "float hash(vec2 p){\n"
        "  p = vec2(dot(p,vec2(127.1,311.7)), dot(p,vec2(269.5,183.3)));\n"
        "  return -1.0 + 2.0*fract(sin(p)*43758.5453);\n"
        "}\n"
        "float noise(vec2 p){\n"
        "  vec2 i=floor(p), f=fract(p);\n"
        "  float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));\n"
        "  vec2 u=f*f*(3.0-2.0*f);\n"
        "  return mix(mix(a,b,u.x), mix(c,d,u.x), u.y)*0.5+0.5;\n"
        "}\n"
        "float fbm(vec2 p){\n"
        "  float s=0.0, a=0.5;\n"
        "  for(int i=0;i<5;i++){ s+=a*noise(p); p*=2.02; a*=0.55; }\n"
        "  return s;\n"
        "}\n"

        "void main(){\n"
        "  vec2 p = gl_FragCoord.xy - uCenter;\n"
        "  float d = length(p);\n"
        "  if(d>uOuterR){ discard; }\n"

        // 渦っぽい座標歪み
        "  float ang = atan(p.y, p.x);\n"
        "  float swirl = uSwirl * smoothstep(uInnerR, uOuterR, d);\n"
        "  ang += swirl * 0.25 * sin(0.6*uTime + d*0.02);\n"
        "  float rr = d;\n"
        "  vec2 q = vec2(cos(ang), sin(ang))*rr;\n"

        // 正規化＆時間スクロールで煙の流れ
        "  vec2 uv = q / max(uOuterR,1.0);\n"
        "  uv *= uNoiseScale;\n"
        "  uv += vec2(0.15*uTime, -0.10*uTime);\n"

        // FBMノイズ（地の濃度）
        "  float n = fbm(uv);\n"

        // 内側は濃く、外側へなめらかに減衰
        "  float core = 1.0 - smoothstep(uInnerR-2.0, uInnerR+2.0, d);\n"
        "  float halo = 1.0 - smoothstep(uInnerR, uOuterR, d);\n"

        // ノイズでムラを作り、内側優先でアルファを作る
        "  float a = clamp( core*0.85 + halo*(0.35 + 0.55*n), 0.0, 1.0 );\n"

        // 少し“呼吸”する明滅
        "  float breathe = 0.9 + 0.1*sin(uTime*2.2);\n"
        "  vec3 col = uColor * uIntensity * (0.8 + 0.2*n) * breathe;\n"
        "  gl_FragColor = vec4(col, a);\n"
        "}\n";

    GLuint compile(GLenum type, const char* src){
        GLuint s = pglCreateShader(type);
        pglShaderSource(s, 1, &src, nullptr);
        pglCompileShader(s);
        GLint ok=0; pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if(!ok){
            char log[1024]; GLsizei n=0; pglGetShaderInfoLog(s,1024,&n,log);
            std::fprintf(stderr,"[PlayerSmoke] shader compile error:\n%.*s\n",(int)n,log);
            pglDeleteShader(s); return 0;
        }
        return s;
    }
}

namespace PlayerSmokeShader {
    bool EnsureBuilt(){
        if(gBuilt) return gOK;
        gBuilt = true;

        // OpenGL コンテキスト作成・Current 後に呼ばれていること！
        if(!loadGL2Funcs()){
            std::fprintf(stderr,"[PlayerSmoke] GL2 functions missing.\n");
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
            std::fprintf(stderr,"[PlayerSmoke] program link error:\n%.*s\n",(int)n,log);
            gOK=false; return false;
        }

        uCenter     = pglGetUniformLocation(gProg, "uCenter");
        uInnerR     = pglGetUniformLocation(gProg, "uInnerR");
        uOuterR     = pglGetUniformLocation(gProg, "uOuterR");
        uTime       = pglGetUniformLocation(gProg, "uTime");
        uColor      = pglGetUniformLocation(gProg, "uColor");
        uIntensity  = pglGetUniformLocation(gProg, "uIntensity");
        uNoiseScale = pglGetUniformLocation(gProg, "uNoiseScale");
        uSwirl      = pglGetUniformLocation(gProg, "uSwirl");
        gOK = true;
        return true;
    }

    bool IsSupported(){ return EnsureBuilt(); }

    void Use(){ if(gOK) pglUseProgram(gProg); }
    void Stop(){ if(gOK) pglUseProgram(0); }

    void SetUniforms(float cx, float cy,
                     float innerR, float outerR,
                     float time,
                     float r, float g, float b,
                     float intensity,
                     float noiseScale,
                     float swirl){
        if(!gOK) return;
        pglUniform2f(uCenter, cx, cy);
        pglUniform1f(uInnerR, innerR);
        pglUniform1f(uOuterR, outerR);
        pglUniform1f(uTime,   time);
        pglUniform3f(uColor,  r, g, b);
        pglUniform1f(uIntensity,  intensity);
        pglUniform1f(uNoiseScale, noiseScale);
        pglUniform1f(uSwirl,      swirl);
    }
}
