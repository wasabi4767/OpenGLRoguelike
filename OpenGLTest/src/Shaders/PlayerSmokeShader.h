#pragma once
#include <GLFW/glfw3.h>

namespace PlayerSmokeShader {
    bool EnsureBuilt();
    bool IsSupported();

    void Use();
    void Stop();

    // center: 画面ピクセル座標
    // innerR: コア半径（完全に不透明に近い領域）
    // outerR: 煙の最外周
    // color : 煙色（0..1）
    // intensity: 明るさスケール（0.6～1.6推奨）
    // noiseScale: 煙の粒度（0.6～2.0推奨、小さいほど粗く大きい渦）
    // swirl: 渦の強さ（0～2.5推奨）
    void SetUniforms(float centerX, float centerY,
                     float innerR, float outerR,
                     float time,
                     float r, float g, float b,
                     float intensity,
                     float noiseScale,
                     float swirl);
}
