#pragma once
#include "Player.h"
#include <GLFW/glfw3.h>
#include <vector>

// 前方宣言（ポインタ格納なのでヘッダはここでinclude不要）
class Enemy;
class Bullet;
class XPOrb;

class Game {
public:
    Game(int width, int height, const char* title);
    ~Game();
    void Run();

private:
    void ProcessInput();
    void Update(float deltaTime);
    void Render();

    // レベルアップUI/適用
    void OpenLevelUpMenu();
    void ApplyLevelUpChoice(int code);             // ←「位置」ではなく「コード」を受け取る
    void RenderLevelUpOverlay();        // 透過パネルと三つの枠を描画
    
    void Shoot();        // 自動射撃
    void SpawnEnemy();   // 敵スポーン

    GLFWwindow* window;
    Player* player;
    
    std::vector<Enemy*> enemies;
    std::vector<Bullet*> bullets;
    std::vector<XPOrb*> xpOrbs;

    // レベルアップ候補のコード定義
    enum LevelUpCode {
        LUC_FIRE_RATE = 1, // 発射レート
        LUC_BULLET    = 2, // 弾数+1
        LUC_HEAL      = 3, // 体力回復
        LUC_SPEED     = 4  // 移動速度上昇
    };
    
    
    int winW = 800, winH = 600;
    int playerHP = 5, playerMaxHP = 5;
    int playerLevel = 1, playerXP = 0, xpToNext = 5;

    float spawnTimer = 0.0f;
    float spawnInterval = 1.2f; // 徐々に短縮して難易度UP

    float fireCooldown = 0.0f;
    float fireInterval = 0.5f;  // 連射間隔（レベルアップで短縮予定）
    int   projectilesPerShot = 1; // 同時発射弾数（レベルアップで増加予定）

    float contactDamageCooldown = 0.6f;
    float contactDamageTimer = 0.0f;

    bool  isPausedForLevelUp = false;   // レベルアップ選択中の一時停止フラグ
    bool  needLevelUpKeyRelease = false; // メニューを開いた直後の押しっぱ対策
    int   pendingLevelUps = 0;          // 同時に複数上がった場合の保留数

    // レベルアップ時の上昇量
    float levelup_fire_rate_multiplier = 0.85f; // 発射レート+15% ≒ fireIntervalを0.85倍
    int   levelup_heal_amount = 1;              // 体力+1/回

    int  levelUpChoices[3] = { LUC_FIRE_RATE, LUC_BULLET, LUC_HEAL }; // 提示する選択肢

    struct Rect { float x0, y0, x1, y1; };
    Rect levelUpBoxes[3];         // 3つの選択枠の矩形
    int  hoverChoice = 0;         // ホバー中の枠（0=なし 1/2/3）
    bool isMouseDownL = false;    // 左クリック押下の前フレーム状態
    
    // レイアウト/判定ヘルパ
    void ComputeLevelUpLayout();
    bool PointInRect(float x, float y, const Rect& r);
    void RollLevelUpChoices();                     // 候補から3件をランダム抽選
    const char* ChoiceLabel(int code) const;       // タイトル/ログ用のラベル
    

};
