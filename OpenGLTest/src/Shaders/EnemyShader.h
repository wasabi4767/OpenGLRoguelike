#pragma once
#include <GLFW/glfw3.h>

namespace EnemyShader {
    bool EnsureBuilt();
    bool IsSupported();
    void Use();
    void Stop();

    // center : 画面ピクセル座標（敵の中心）
    // size   : 三角のスケール（高さ≒size、底辺≒size*0.9 くらい）
    // dir    : 先端の向き（Player 方向の正規化ベクトル）
    // colA/B : 内側→縁の2色グラデ
    // intensity : 発光強度（0.7～1.6）
    void SetUniforms(float centerX, float centerY,
                     float size,
                     float time,
                     float dirX, float dirY,
                     float colAx, float colAy, float colAz,
                     float colBx, float colBy, float colBz,
                     float intensity);
}

