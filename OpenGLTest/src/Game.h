#pragma once

#include <vector>

// 前方宣言（循環依存の回避）
struct GLFWwindow;
class Player;
class Enemy;
class Bullet;
class XPOrb;

// レベルアップ選択肢コード
enum LevelUpCode {
    LUC_FIRE_RATE = 0,   // 発射レート上昇（発射間隔を短縮）
    LUC_BULLET   = 1,    // 同時発射弾数 +1
    LUC_HEAL     = 2,    // HP 回復
    LUC_SPEED    = 3,    // プレイヤー移動速度上昇
};

// 2D矩形（レベルアップUIのヒットテストに使用）
struct Rect {
    float x0, y0, x1, y1;
};

class Game {
public:
    Game(int width, int height, const char* title);
    ~Game();

    void Run();

private:
    // メインループの各フェーズ
    void ProcessInput();
    void Update(float dt);
    void Render();

    // ゲームロジック
    void Shoot();
    void SpawnEnemy();

    // レベルアップUI
    void OpenLevelUpMenu();
    void ApplyLevelUpChoice(int code);
    const char* ChoiceLabel(int code) const;
    void RollLevelUpChoices();   // 4候補から重複なしで3つ抽選
    void RenderLevelUpOverlay(); // 半透明背景 + ボックス
    void ComputeLevelUpLayout(); // levelUpBoxes を計算
    bool PointInRect(float x, float y, const Rect& r);

private:
    // ウィンドウ / ビューポート
    GLFWwindow* window = nullptr;
    int winW = 0, winH = 0;

    // プレイヤー＆エンティティ
    Player* player = nullptr;
    std::vector<Enemy*>  enemies;
    std::vector<Bullet*> bullets;
    std::vector<XPOrb*>  xpOrbs;

    // 発射関連
    float fireInterval = 0.35f;     // 発射間隔（秒）
    float fireCooldown = 0.0f;      // 次弾までの残り時間
    int   projectilesPerShot = 1;   // 同時発射弾数（+1強化で増える）

    // 敵スポーン
    float spawnInterval = 1.30f;    // 敵出現間隔（徐々に短縮、下限あり）
    float spawnTimer    = 0.0f;

    // プレイヤー耐久
    int playerMaxHP = 5;
    int playerHP    = 5;

    // 近接接触ダメージのクールダウン（連続ヒット抑制）
    float contactDamageCooldown = 0.55f;
    float contactDamageTimer    = 0.0f;

    // XP / レベル
    int playerXP   = 0;
    int xpToNext   = 5;     // 次レベルまで
    int playerLevel = 1;
    int pendingLevelUps = 0;

    // レベルアップUI状態
    bool isPausedForLevelUp     = false; // 表示中はゲーム時間停止
    bool needLevelUpKeyRelease  = false; // 押しっぱなし誤選択防止
    bool isMouseDownL           = false; // マウス左ボタンのエッジ検出
    int  hoverChoice            = 0;     // 1/2/3（0=なし）
    int  levelUpChoices[3]      = { LUC_FIRE_RATE, LUC_BULLET, LUC_HEAL };
    Rect levelUpBoxes[3]        = {};    // 3つのボックス矩形

    // レベルアップ効果パラメータ
    const float levelup_fire_rate_multiplier = 0.85f; // 間隔×0.85（=速く）
    const int   levelup_heal_amount          = 1;     // HP +1

#if defined(_WIN32)
public:
    // -------- BGM 制御（MCI / winmm）--------
    void InitBGM(const char* pathUtf8);   // 例: "Audio/bgm.mp3"
    void PlayBGM(bool loop);
    void PauseBGM();
    void ResumeBGM();
    void StopBGM();
    void SetBGMVolume(int vol0to1000);    // 0～1000

private:
    bool bgmReady   = false;
    bool bgmLooping = false;
    int  bgmVolumeNormal = 700; // 通常時の音量
    int  bgmVolumePaused = 400; // レベルアップ選択中の音量
#endif
};
