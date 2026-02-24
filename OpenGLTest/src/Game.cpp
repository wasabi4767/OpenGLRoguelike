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

#endif

    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

Game::~Game() {
    for (auto* e : enemies) delete e;
    for (auto* b : bullets) delete b;
    for (auto* o : xpOrbs)  delete o;
    delete player;

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
    // HUD（HPなど）
    RenderHUD();

    if (isPausedForLevelUp) {
        RenderLevelUpOverlay(); // 画像は保留中のため、枠とテキストのみ
    }
}

// ==============================
// HUD：HP（ハート）
// ==============================
static void DrawHeartHUD(float cx, float cy, float s, bool filled)
{
    // 2つの円 + 下三角でハート風（2D）
    // cx,cy は中心。s は大きさ。

    // 形状パラメータ
    float r0 = s * 0.22f; // 上の丸半径
    float ox = s * 0.22f; // 左右の丸オフセット
    float oy = s * 0.08f; // 丸の上下

    // 塗り
    if (filled) {
        glColor4f(1.0f, 0.40f, 0.60f, 0.95f);

        // 左丸
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx - ox, cy + oy);
        for (int i = 0; i <= 24; ++i) {
            float a = (float)i / 24.0f * 6.2831853f;
            glVertex2f((cx - ox) + std::cos(a) * r0, (cy + oy) + std::sin(a) * r0);
        }
        glEnd();

        // 右丸
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx + ox, cy + oy);
        for (int i = 0; i <= 24; ++i) {
            float a = (float)i / 24.0f * 6.2831853f;
            glVertex2f((cx + ox) + std::cos(a) * r0, (cy + oy) + std::sin(a) * r0);
        }
        glEnd();

        // 下（三角）
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - ox - r0 * 1.0f, cy + oy * 0.6f);
        glVertex2f(cx + ox + r0 * 1.0f, cy + oy * 0.6f);
        glVertex2f(cx, cy - s * 0.40f);
        glEnd();
    }

    // 輪郭（塗りなしの時も見えるように）
    glColor4f(1.f, 1.f, 1.f, filled ? 0.85f : 0.45f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);

    // ハートのパラメトリック（見た目が良い）
    // x = 16 sin^3(t)
    // y = 13 cos(t) - 5 cos(2t) - 2 cos(3t) - cos(4t)
    for (int i = 0; i < 80; ++i) {
        float t = (float)i / 80.0f * 6.2831853f;
        float st = std::sin(t);
        float ct = std::cos(t);
        float x = 16.0f * st * st * st;
        float y = 13.0f * ct - 5.0f * std::cos(2.0f * t) - 2.0f * std::cos(3.0f * t) - std::cos(4.0f * t);

        // 正規化スケール
        float k = s * 0.020f;
        glVertex2f(cx + x * k, cy + y * k);
    }

    glEnd();
    glLineWidth(1.0f);
}

void Game::RenderHUD()
{
    // 左上に HP をハートで表示
    // ※このプロジェクトは glOrtho(0..W, 0..H) で、左下原点。
    const float margin = 16.0f;
    const float heartS = 28.0f;
    const float gap = 10.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float y = winH - margin - heartS * 0.5f;
    for (int i = 0; i < playerMaxHP; ++i) {
        float x = margin + heartS * 0.5f + i * (heartS + gap);
        bool filled = (i < playerHP);
        DrawHeartHUD(x, y, heartS, filled);
    }

    glDisable(GL_BLEND);
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

// ==============================
// レベルアップ用：画像なしアイコン（図形）
// ==============================
static void DrawUpgradeIconGL(int code, const Rect& r, bool hovered)
{
    // アイコン描画領域（箱の左上 60x60）
    float pad = 14.0f;
    float s = hovered ? 66.0f : 60.0f;

    float x0 = r.x0 + pad;
    float y0 = r.y0 + pad;
    float x1 = x0 + s;
    float y1 = y0 + s;

    float cx = (x0 + x1) * 0.5f;
    float cy = (y0 + y1) * 0.5f;

    // 背景プレート（薄い黒）
    glColor4f(0.f, 0.f, 0.f, 0.18f);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0); glVertex2f(x1, y0);
    glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();

    // 共通：輪郭
    glColor4f(1.f, 1.f, 1.f, 0.55f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0); glVertex2f(x1, y0);
    glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();

    // codeごとに簡易アイコン
    switch (code) {
    case LUC_HEAL: {
        // ハート（赤すぎない：ピンク寄り）
        glColor4f(1.0f, 0.45f, 0.65f, 0.95f);
        // 2つの円っぽいもの＋下三角でハート風
        float r0 = s * 0.18f;
        // 左丸
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx - r0, cy + r0 * 0.4f);
        for (int i = 0;i <= 24;i++) {
            float a = (float)i / 24.0f * 6.2831853f;
            glVertex2f((cx - r0) + std::cos(a) * r0, (cy + r0 * 0.4f) + std::sin(a) * r0);
        }
        glEnd();
        // 右丸
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx + r0, cy + r0 * 0.4f);
        for (int i = 0;i <= 24;i++) {
            float a = (float)i / 24.0f * 6.2831853f;
            glVertex2f((cx + r0) + std::cos(a) * r0, (cy + r0 * 0.4f) + std::sin(a) * r0);
        }
        glEnd();
        // 下の三角
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - r0 * 2.1f, cy + r0 * 0.2f);
        glVertex2f(cx + r0 * 2.1f, cy + r0 * 0.2f);
        glVertex2f(cx, cy - r0 * 2.6f);
        glEnd();
    } break;

    case LUC_SPEED: {
        // 速度：ダッシュ矢印＋スピードライン（青緑）
        glColor4f(0.35f, 0.95f, 0.85f, 0.95f);
        float w = s * 0.30f;
        float h = s * 0.18f;

        // 矢印本体（右向き）
        glBegin(GL_TRIANGLES);
        glVertex2f(cx + w, cy);
        glVertex2f(cx - w, cy + h);
        glVertex2f(cx - w, cy - h);
        glEnd();

        // スピードライン
        glColor4f(0.35f, 0.95f, 0.85f, 0.65f);
        glBegin(GL_LINES);
        glVertex2f(x0 + s * 0.12f, cy + s * 0.18f); glVertex2f(cx - w * 0.2f, cy + s * 0.18f);
        glVertex2f(x0 + s * 0.08f, cy);           glVertex2f(cx - w * 0.5f, cy);
        glVertex2f(x0 + s * 0.12f, cy - s * 0.18f); glVertex2f(cx - w * 0.2f, cy - s * 0.18f);
        glEnd();
    } break;

    case LUC_BULLET: {
        // 弾数＋1：2発弾＋小さな「+」
        glColor4f(1.0f, 0.85f, 0.25f, 0.95f);
        float bw = s * 0.16f;
        float bh = s * 0.34f;

        auto drawBullet = [&](float bx, float by) {
            // 本体
            glBegin(GL_QUADS);
            glVertex2f(bx - bw, by - bh * 0.4f);
            glVertex2f(bx + bw, by - bh * 0.4f);
            glVertex2f(bx + bw, by + bh * 0.4f);
            glVertex2f(bx - bw, by + bh * 0.4f);
            glEnd();
            // 先端
            glBegin(GL_TRIANGLES);
            glVertex2f(bx - bw, by + bh * 0.4f);
            glVertex2f(bx + bw, by + bh * 0.4f);
            glVertex2f(bx, by + bh * 0.7f);
            glEnd();
            };

        drawBullet(cx - s * 0.14f, cy - s * 0.05f);
        drawBullet(cx + s * 0.10f, cy + s * 0.05f);

        // + マーク
        glColor4f(1.f, 1.f, 1.f, 0.95f);
        float px = x1 - s * 0.18f;
        float py = y0 + s * 0.20f;
        float ps = s * 0.10f;
        glBegin(GL_LINES);
        glVertex2f(px - ps, py); glVertex2f(px + ps, py);
        glVertex2f(px, py - ps); glVertex2f(px, py + ps);
        glEnd();
    } break;

    case LUC_FIRE_RATE: {
        // 発射レート：弾＋「>>」みたいな加速マーク（オレンジ寄り）
        glColor4f(1.0f, 0.55f, 0.20f, 0.95f);
        float bw = s * 0.14f;
        float bh = s * 0.32f;

        // 弾（縦）
        glBegin(GL_QUADS);
        glVertex2f(cx - s * 0.18f - bw, cy - bh * 0.4f);
        glVertex2f(cx - s * 0.18f + bw, cy - bh * 0.4f);
        glVertex2f(cx - s * 0.18f + bw, cy + bh * 0.4f);
        glVertex2f(cx - s * 0.18f - bw, cy + bh * 0.4f);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - s * 0.18f - bw, cy + bh * 0.4f);
        glVertex2f(cx - s * 0.18f + bw, cy + bh * 0.4f);
        glVertex2f(cx - s * 0.18f, cy + bh * 0.7f);
        glEnd();

        // 「>>」加速マーク
        glColor4f(1.f, 1.f, 1.f, 0.9f);
        float ax = cx + s * 0.05f;
        float ay = cy;
        float aw = s * 0.20f;
        float ah = s * 0.14f;

        glBegin(GL_TRIANGLES);
        glVertex2f(ax, ay);
        glVertex2f(ax - aw * 0.5f, ay + ah);
        glVertex2f(ax - aw * 0.5f, ay - ah);
        glVertex2f(ax + aw * 0.45f, ay);
        glVertex2f(ax, ay + ah);
        glVertex2f(ax, ay - ah);
        glEnd();
    } break;

    default:
        break;
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

        // ※ 画像は現在保留。
        bool hovered = (hoverChoice == idx + 1);
        int code = levelUpChoices[idx];
        DrawUpgradeIconGL(code, r, hovered);

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

#endif
