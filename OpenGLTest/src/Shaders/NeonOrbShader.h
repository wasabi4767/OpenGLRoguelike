#pragma once
#include <GLFW/glfw3.h>

namespace NeonOrbShader {
    // 初期化（必要時に一度だけ実行）
    bool EnsureBuilt();

    // シェーダを有効化
    void Use();

    // シェーダを無効化
    void Stop();

    // ユニフォーム設定（画面座標・ピクセル単位）
    // center: 画面(ピクセル)座標、innerR: コア半径、outerR: ハロ外周半径
    // color: 発光色 (0..1)
    void SetUniforms(float centerX, float centerY, float innerR, float outerR,
                     float time, float r, float g, float b);

    // 失敗時（古い環境等）に固定機能へフォールバックするか
    bool IsSupported();
}
