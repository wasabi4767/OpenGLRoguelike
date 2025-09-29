#pragma once
#include <GLFW/glfw3.h>

namespace PlayerFlareShader {
    bool EnsureBuilt();
    bool IsSupported();

    void Use();
    void Stop();

    // center: 画面ピクセル座標
    // innerR: コア半径
    // outerR: ハロ外周半径
    // colA/colB: グラデ用の2色
    // intensity: 発光強度
    // hitAmt: 被弾赤フラッシュ量(0..1) / invOn: 無敵点滅フラグ(0 or 1)
    void SetUniforms(float centerX, float centerY,
                     float innerR, float outerR,
                     float time,
                     float colAx, float colAy, float colAz,
                     float colBx, float colBy, float colBz,
                     float intensity,
                     float hitAmt,
                     float invOn);
}

