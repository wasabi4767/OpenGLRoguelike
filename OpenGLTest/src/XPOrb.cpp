#include "XPOrb.h"
#include "Shaders/NeonOrbShader.h"
#include <GLFW/glfw3.h>
#include <cmath>

void XPOrb::Draw() const {
    if (!alive) return;

    // コア半径とハロ半径
    float inner = radius;          // コア
    float outer = radius * 2.4f;   // ハロ
    float t = (float)glfwGetTime();

    // ブレンド状態を保存し、加算合成に切り替え
    GLboolean wasBlend = glIsEnabled(GL_BLEND);
    GLint prevSrc = 0, prevDst = 0;
    glGetIntegerv(GL_BLEND_SRC, &prevSrc);
    glGetIntegerv(GL_BLEND_DST, &prevDst);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // 加算

    if (NeonOrbShader::IsSupported()) {
        // --- シェーダ有効化 ---
        NeonOrbShader::Use();
        // 色の設定(シアン寄り)
        NeonOrbShader::SetUniforms(x, y, inner, outer, t, 0.2f, 0.8f, 1.0f);

        // 画面座標評価なので、オーブ中心を覆うだけの四角を描けばOK
        float R = outer + 8.0f; // ちょい余白
        glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(x - R, y - R);
        glVertex2f(x + R, y - R);
        glVertex2f(x - R, y + R);
        glVertex2f(x + R, y + R);
        glEnd();

        NeonOrbShader::Stop();
    } 
    glDisable(GL_BLEND);
}


bool XPOrb::CheckPickup(float px, float py, float pickupR) {
    if (!alive) return false;
    float dx = px - x;
    float dy = py - y;
    float r = pickupR + radius;
    if (dx*dx + dy*dy <= r*r) { alive = false; return true; }
    return false;
}

