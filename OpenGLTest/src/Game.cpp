#include "Game.h"
#include "Enemy.h"
#include "Bullet.h"
#include "XPOrb.h"
#include "Player.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <iostream>

// ---- std::max/min を使わない代替 ----
#ifndef MAX2
#define MAX2(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN2
#define MIN2(a,b) (((a) < (b)) ? (a) : (b))
#endif

// ============================================================
// Windows: BGM (MCI) 用
// ============================================================
#if defined(_WIN32)
  #define NOMINMAX
  #include <Windows.h>
  #include <mmsystem.h>
  #pragma comment(lib, "winmm.lib")

  // 実行ファイルのあるディレクトリへ CWD を移動（相対パス安定化）
  static void SetCWDToExeDir() {
      wchar_t path[MAX_PATH];
      DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
      if (n == 0 || n >= MAX_PATH) return;
      for (int i = (int)n - 1; i >= 0; --i) {
          if (path[i] == L'\\' || path[i] == L'/') { path[i] = L'\0'; break; }
      }
      SetCurrentDirectoryW(path);
  }

  // UTF-8 → UTF-16
  static std::wstring Utf8ToW(const char* s) {
      if (!s) return L"";
      int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
      std::wstring w(n, L'\0');
      MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
      if (!w.empty() && w.back() == L'\0') w.pop_back();
      return w;
  }
#endif

// ============================================================
// Game 本体
// ============================================================
Game::Game(int width, int height, const char* title) {
    glfwInit();
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    winW = width; winH = height;

    player = new Player(width, height);

#if defined(_WIN32)
    // 実行ファイルの場所を基準に相対パスを解決
    SetCWDToExeDir();

    // ---- BGM 起動（必要なら wav でも可：InitBGM("Audio/bgm.wav")）----
    InitBGM("Audio/bgm.mp3");
    PlayBGM(true);                 // ループ再生
    SetBGMVolume(bgmVolumeNormal); // 初期音量
#endif

    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

Game::~Game() {
    for (auto* e : enemies) delete e;
    for (auto* b : bullets) delete b;
    for (auto* o : xpOrbs)  delete o;
    delete player;

#if defined(_WIN32)
    StopBGM(); // MCI を閉じる
#endif

    glfwTerminate();
}

void Game::Run() {
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        ProcessInput();
        Update(dt);
        Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Game::ProcessInput() {
    if (isPausedForLevelUp) return; // レベルアップ選択中は移動不可

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) player->Move(-1, 0);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) player->Move( 1, 0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) player->Move( 0, 1);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) player->Move( 0,-1);
}

void Game::Update(float dt) {

    // ---- レベルアップメニュー中（ゲーム時間停止）----
    if (isPausedForLevelUp) {
        ComputeLevelUpLayout();

        double mx, my; glfwGetCursorPos(window, &mx, &my);
        float fx = (float)mx;
        float fy = (float)(winH - my);

        hoverChoice = 0;
        if (PointInRect(fx, fy, levelUpBoxes[0])) hoverChoice = 1;
        else if (PointInRect(fx, fy, levelUpBoxes[1])) hoverChoice = 2;
        else if (PointInRect(fx, fy, levelUpBoxes[2])) hoverChoice = 3;

        int  k1 = glfwGetKey(window, GLFW_KEY_1);
        int  k2 = glfwGetKey(window, GLFW_KEY_2);
        int  k3 = glfwGetKey(window, GLFW_KEY_3);
        bool lmbNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

        if (needLevelUpKeyRelease) {
            if (k1 != GLFW_PRESS && k2 != GLFW_PRESS && k3 != GLFW_PRESS && !lmbNow) {
                needLevelUpKeyRelease = false;
            }
        } else {
            if (hoverChoice != 0 && lmbNow && !isMouseDownL) {
                ApplyLevelUpChoice(levelUpChoices[hoverChoice - 1]);
            } else if (k1 == GLFW_PRESS) {
                ApplyLevelUpChoice(levelUpChoices[0]);
            } else if (k2 == GLFW_PRESS) {
                ApplyLevelUpChoice(levelUpChoices[1]);
            } else if (k3 == GLFW_PRESS) {
                ApplyLevelUpChoice(levelUpChoices[2]);
            }
        }
        isMouseDownL = lmbNow;
        return; // ポーズなので他は更新しない
    }

    // ---- 通常更新 ----
    player->Update(dt);

    // 自動射撃
    fireCooldown -= dt;
    if (fireCooldown <= 0.0f) {
        Shoot();
        fireCooldown = fireInterval;
    }

    // 敵スポーン
    spawnTimer += dt;
    if (spawnTimer >= spawnInterval) {
        SpawnEnemy();
        spawnTimer = 0.0f;
        if (spawnInterval > 0.35f) spawnInterval *= 0.985f; // 徐々に短縮（下限あり）
    }

    // 弾・敵更新
    for (auto* b : bullets) if (b) b->Update(dt);
    for (auto* e : enemies) if (e) e->Update(dt, player->GetX(), player->GetY());

    // 弾→敵ヒット
    for (auto* b : bullets) {
        if (!b || !b->IsAlive()) continue;
        for (auto* e : enemies) {
            if (!e || !e->IsAlive()) continue;
            if (b->Hit(e->X(), e->Y(), e->Size())) {
                e->Damage(b->Damage());
                b->Kill();
                break;
            }
        }
    }

    // 敵死亡：XPドロップ
    for (auto*& e : enemies) {
        if (!e) continue;
        if (!e->IsAlive()) {
            xpOrbs.push_back(new XPOrb(e->X(), e->Y(), 1));
            delete e; e = nullptr;
        }
    }

    // プレイヤー接触ダメージ（無敵は免疫、復帰直後の理不尽を抑える）
    contactDamageTimer -= dt;
    if (contactDamageTimer < 0.0f) contactDamageTimer = 0.0f;

    if (contactDamageTimer <= 0.0f) {
        float pr = player->GetSize();
        bool overlapped = false;

        for (auto* e : enemies) {
            if (!e) continue;
            float dx = player->GetX() - e->X();
            float dy = player->GetY() - e->Y();
            float rr = pr + e->Size();
            if (dx*dx + dy*dy <= rr*rr) { overlapped = true; break; }
        }

        if (overlapped) {
            if (player->IsInvincible()) {
                contactDamageTimer = 0.08f; // 無敵中は小刻みグレースを維持
            } else {
                playerHP -= 1;
                player->OnHit(1);
                contactDamageTimer = contactDamageCooldown;
                std::cout << "HP: " << playerHP << "/" << playerMaxHP << std::endl;
            }
        }
    }

    // XPオーブ取得
    for (auto*& o : xpOrbs) {
        if (!o) continue;
        if (o->CheckPickup(player->GetX(), player->GetY(), 16.0f)) {
            playerXP += o->Amount();
            delete o; o = nullptr;
        }
    }

    // レベルアップ判定
    while (playerXP >= xpToNext) {
        playerXP   -= xpToNext;
        playerLevel += 1;
        xpToNext = (int)(xpToNext * 1.7f);
        pendingLevelUps += 1;
        std::cout << "LEVEL UP! Pending=" << pendingLevelUps << "\n";
    }
    if (pendingLevelUps > 0 && !isPausedForLevelUp) {
        OpenLevelUpMenu();
    }

    // 後始末
    enemies.erase(std::remove(enemies.begin(), enemies.end(), nullptr), enemies.end());
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                    [](Bullet* b){ return !b || !b->IsAlive(); }), bullets.end());
    xpOrbs.erase(std::remove(xpOrbs.begin(), xpOrbs.end(), nullptr), xpOrbs.end());

    // ゲームオーバー
    if (playerHP <= 0) {
        std::cout << "GAME OVER" << std::endl;
        glfwSetWindowShouldClose(window, true);
    }
}

void Game::Render() {
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (auto* o : xpOrbs)  if (o) o->Draw();
    for (auto* e : enemies) if (e) e->Draw();
    for (auto* b : bullets) if (b) b->Draw();
    player->Draw();

    if (isPausedForLevelUp) {
        RenderLevelUpOverlay(); // 画像は保留中のため、枠とテキストのみ
    }
}

void Game::Shoot() {
    float px = player->GetX();
    float py = player->GetY();
    const float speed = 260.0f;
    const float life  = 1.3f;

    if (projectilesPerShot >= 0) {
        float vx = 0.f, vy = speed;
        if (!enemies.empty()) {
            Enemy* nearest = nullptr;
            float bestD2 = 1e9f;
            for (auto* e : enemies) {
                if (!e) continue;
                float dx = e->X() - px;
                float dy = e->Y() - py;
                float d2 = dx*dx + dy*dy;
                if (d2 < bestD2) { bestD2 = d2; nearest = e; }
            }
            if (nearest) {
                float dx = nearest->X() - px;
                float dy = nearest->Y() - py;
                float len = std::sqrt(dx*dx + dy*dy);
                if (len > 0.0001f) { dx /= len; dy /= len; }
                vx = dx * speed; vy = dy * speed;
            }
        }
        bullets.push_back(new Bullet(px, py, vx, vy, life, 1));
    }
}

void Game::SpawnEnemy() {
    int side = std::rand() % 4; // 0:下,1:上,2:左,3:右
    float x = 0.f, y = 0.f;
    switch (side) {
        case 0: x = (float)(std::rand() % winW); y = -20.f; break;
        case 1: x = (float)(std::rand() % winW); y = winH + 20.f; break;
        case 2: x = -20.f; y = (float)(std::rand() % winH); break;
        case 3: x = winW + 20.f; y = (float)(std::rand() % winH); break;
    }
    float spd = 60.f + (std::rand() % 30); // 60~90
    float size = 10.f;
    int hp = 2;
    enemies.push_back(new Enemy(x, y, spd, size, hp));
}

void Game::OpenLevelUpMenu() {
    isPausedForLevelUp    = true;
    needLevelUpKeyRelease = true;
    hoverChoice           = 0;

    RollLevelUpChoices();

#if defined(_WIN32)
    // メニュー中は少し音量を下げる（好みで Pause/Resume にしてもOK）
    SetBGMVolume(bgmVolumePaused);
#endif

    char title[256];
    std::snprintf(title, sizeof(title),
        "LEVEL UP!  [1] %s   [2] %s   [3] %s",
        ChoiceLabel(levelUpChoices[0]),
        ChoiceLabel(levelUpChoices[1]),
        ChoiceLabel(levelUpChoices[2]));
    glfwSetWindowTitle(window, title);

    std::cout << "\n=== LEVEL UP! ===\n"
              << "[1] " << ChoiceLabel(levelUpChoices[0]) << "\n"
              << "[2] " << ChoiceLabel(levelUpChoices[1]) << "\n"
              << "[3] " << ChoiceLabel(levelUpChoices[2]) << "\n";
}

void Game::ApplyLevelUpChoice(int code) {
    switch (code) {
    case LUC_FIRE_RATE:
        fireInterval = MAX2(0.10f, fireInterval * levelup_fire_rate_multiplier);
        std::cout << "Applied: Fire Rate Up -> interval=" << fireInterval << "\n";
        break;
    case LUC_BULLET:
        projectilesPerShot += 1;
        std::cout << "Applied: +1 Bullet -> count=" << projectilesPerShot << "\n";
        break;
    case LUC_HEAL:
        playerHP = MIN2(playerMaxHP, playerHP + levelup_heal_amount);
        std::cout << "Applied: Heal +" << levelup_heal_amount
                  << " -> HP " << playerHP << "/" << playerMaxHP << "\n";
        break;
    case LUC_SPEED:
        player->MultiplySpeed(1.15f);
        std::cout << "Applied: Move Speed +15% -> speed=" << player->GetSpeed() << "\n";
        break;
    default: break;
    }

    pendingLevelUps = MAX2(0, pendingLevelUps - 1);
    if (pendingLevelUps > 0) {
        OpenLevelUpMenu();
    } else {
        isPausedForLevelUp = false;
#if defined(_WIN32)
        SetBGMVolume(bgmVolumeNormal);
#endif
        glfwSetWindowTitle(window, "OpenGL Game");
    }
}

const char* Game::ChoiceLabel(int code) const {
    switch (code) {
    case LUC_FIRE_RATE: return "Fire Rate +15%";
    case LUC_BULLET:    return "+1 Bullet";
    case LUC_HEAL:      return "Heal +1";
    case LUC_SPEED:     return "Move Speed +15%";
    default:            return "?";
    }
}

// 4候補から重複なしで3つ抽選
void Game::RollLevelUpChoices() {
    int pool[4] = { LUC_FIRE_RATE, LUC_BULLET, LUC_HEAL, LUC_SPEED };
    for (int i = 0; i < 3; ++i) {
        int j = std::rand() % (4 - i);
        levelUpChoices[i] = pool[j];
        int tmp = pool[j]; pool[j] = pool[3 - i]; pool[3 - i] = tmp;
    }
}

void Game::RenderLevelUpOverlay() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 背景（半透明の黒）
    glColor4f(0.f,0.f,0.f,0.5f);
    glBegin(GL_QUADS);
    glVertex2f(0,0); glVertex2f((float)winW,0);
    glVertex2f((float)winW,(float)winH); glVertex2f(0,(float)winH);
    glEnd();

    ComputeLevelUpLayout();

    auto drawBox = [&](int idx) {
        const Rect& r = levelUpBoxes[idx];

        // 面
        glColor4f(0.15f,0.15f,0.15f,0.9f);
        glBegin(GL_QUADS);
        glVertex2f(r.x0,r.y0); glVertex2f(r.x1,r.y0);
        glVertex2f(r.x1,r.y1); glVertex2f(r.x0,r.y1);
        glEnd();

        // 枠
        glColor4f(0.95f,0.95f,0.95f,1.f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(r.x0,r.y0); glVertex2f(r.x1,r.y0);
        glVertex2f(r.x1,r.y1); glVertex2f(r.x0,r.y1);
        glEnd();

        if (hoverChoice == idx+1) {
            glBegin(GL_LINE_LOOP);
            glVertex2f(r.x0+2,r.y0+2); glVertex2f(r.x1-2,r.y0+2);
            glVertex2f(r.x1-2,r.y1-2); glVertex2f(r.x0+2,r.y1-2);
            glEnd();
        }

        // ※ 画像は現在保留。必要ならここにテクスチャ貼り処理を復活させる。
    };

    drawBox(0); drawBox(1); drawBox(2);

    glDisable(GL_BLEND);
}

void Game::ComputeLevelUpLayout() {
    float boxW = 180.f, boxH = 100.f, gap = 30.f;
    float cx = winW * 0.5f, cy = winH * 0.55f;

    auto setBox = [&](int idx, float xCenter, float yCenter) {
        float x0 = xCenter - boxW * 0.5f;
        float y0 = yCenter - boxH * 0.5f;
        float x1 = xCenter + boxW * 0.5f;
        float y1 = yCenter + boxH * 0.5f;
        levelUpBoxes[idx] = { x0, y0, x1, y1 };
    };

    setBox(0, cx - boxW - gap*0.5f, cy); // [1]
    setBox(1, cx,                    cy); // [2]
    setBox(2, cx + boxW + gap*0.5f,  cy); // [3]
}

bool Game::PointInRect(float x, float y, const Rect& r) {
    return (x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1);
}

// ============================================================
// BGM (MCI) 実装
// ============================================================
#if defined(_WIN32)
static const wchar_t* kBgmAlias = L"bgm";

void Game::InitBGM(const char* pathUtf8) {
    // 既に open 済みならクローズ
    mciSendStringW(L"close bgm", nullptr, 0, nullptr);

    std::wstring wpath = Utf8ToW(pathUtf8);

    // MP3: mpegvideo / WAV: waveaudio（type 省略でも開ける環境は多い）
    std::wstring cmd = L"open \"" + wpath + L"\" type mpegvideo alias ";
    cmd += kBgmAlias;

    MCIERROR err = mciSendStringW(cmd.c_str(), nullptr, 0, nullptr);
    bgmReady = (err == 0);
    if (!bgmReady) {
        std::fprintf(stderr, "[bgm] open failed: %s\n", pathUtf8);
    }
}

void Game::PlayBGM(bool loop) {
    if (!bgmReady) return;
    std::wstring cmd = L"play ";
    cmd += kBgmAlias;
    if (loop) cmd += L" repeat";
    if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
        bgmLooping = loop;
    }
}

void Game::PauseBGM() {
    if (!bgmReady) return;
    mciSendStringW(L"pause bgm", nullptr, 0, nullptr);
}

void Game::ResumeBGM() {
    if (!bgmReady) return;
    mciSendStringW(L"resume bgm", nullptr, 0, nullptr);
}

void Game::StopBGM() {
    mciSendStringW(L"stop bgm", nullptr, 0, nullptr);
    mciSendStringW(L"close bgm", nullptr, 0, nullptr);
    bgmReady = false;
    bgmLooping = false;
}

void Game::SetBGMVolume(int vol0to1000) {
    if (!bgmReady) return;
    int v = vol0to1000;
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    wchar_t cmd[64];
    std::swprintf(cmd, 64, L"setaudio bgm volume to %d", v);
    mciSendStringW(cmd, nullptr, 0, nullptr);
}
#endif
