#include "Game.h"
#include "Enemy.h"
#include "Bullet.h"
#include "XPOrb.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>


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

    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

Game::~Game() {
    // 後始末
    for (auto* e : enemies) delete e;
    for (auto* b : bullets) delete b;
    for (auto* o : xpOrbs) delete o;
    delete player;
    glfwTerminate();
}

void Game::Run() {
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        ProcessInput();
        Update(deltaTime);
        Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Game::ProcessInput() {
    if (isPausedForLevelUp) {
        // レベルアップ選択中は移動入力を無効化
        return;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) player->Move(-1, 0);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) player->Move(1, 0);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) player->Move(0, 1);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) player->Move(0, -1);
}

void Game::Update(float dt) {

    // ======= ポーズ中の処理 =======
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
            // クリック（ホバー位置 → コードへ変換）
            if (hoverChoice != 0 && lmbNow && !isMouseDownL) {
                ApplyLevelUpChoice(levelUpChoices[hoverChoice - 1]);
            }
            // キー選択（位置固定→コードへ変換）
            else if (k1 == GLFW_PRESS) { ApplyLevelUpChoice(levelUpChoices[0]); }
            else if (k2 == GLFW_PRESS) { ApplyLevelUpChoice(levelUpChoices[1]); }
            else if (k3 == GLFW_PRESS) { ApplyLevelUpChoice(levelUpChoices[2]); }
        }
        isMouseDownL = lmbNow;
        return; // ポーズ中はゲーム停止
    }

    player->Update(dt);


    // ======= 自動射撃 =======
    fireCooldown -= dt;
    if (fireCooldown <= 0.0f) {
        Shoot();
        fireCooldown = fireInterval;
    }

    // ======= 敵スポーン =======
    spawnTimer += dt;
    if (spawnTimer >= spawnInterval) {
        SpawnEnemy();
        spawnTimer = 0.0f;
        // 徐々に難易度UP（下限）
        if (spawnInterval > 0.35f) spawnInterval *= 0.985f;
    }

    // ======= 弾更新・敵当たり判定 =======
    for (auto* b : bullets) if (b) b->Update(dt);

    for (auto* e : enemies) {
        if (!e) continue;
        e->Update(dt, player->GetX(), player->GetY());
    }

    // 弾→敵の衝突
    for (auto* b : bullets) {
        if (!b || !b->IsAlive()) continue;
        for (auto* e : enemies) {
            if (!e || !e->IsAlive()) continue;
            if (b->Hit(e->X(), e->Y(), e->Size())) {
                e->Damage(b->Damage());
                // ここを置き換え
                b->Kill();               // ← 安全に弾を消す
                break;                   // 多重Hitを避ける
            }
        }
    }

    // 敵の死亡処理＆XPドロップ
    for (auto*& e : enemies) {
        if (!e) continue;
        if (!e->IsAlive()) {
            xpOrbs.push_back(new XPOrb(e->X(), e->Y(), 1));
            delete e; e = nullptr;
        }
    }

    // プレイヤー接触ダメージ
    contactDamageTimer -= dt;
    if (contactDamageTimer < 0.0f) contactDamageTimer = 0.0f;

    if (contactDamageTimer <= 0.0f) {
        float pr = player->GetSize();
        bool overlapped = false;
        Enemy* hitEnemy = nullptr;

        for (auto* e : enemies) {
            if (!e) continue;
            float dx = player->GetX() - e->X();
            float dy = player->GetY() - e->Y();
            float rr = pr + e->Size();
            if (dx*dx + dy*dy <= rr*rr) { overlapped = true; hitEnemy = e; break; }
        }

        if (overlapped) {
            if (player->IsInvincible()) {
                contactDamageTimer = 0.08f; 
            } else {
                // 従来どおりのダメージ処理
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

    // レベルアップ処理
    while (playerXP >= xpToNext) {
        playerXP   -= xpToNext;
        playerLevel += 1;
        xpToNext = static_cast<int>(xpToNext * 1.7f);

        pendingLevelUps += 1; // 選択待ちを積む
        std::cout << "LEVEL UP! Pending=" << pendingLevelUps << "\n";
    }
    if (pendingLevelUps > 0 && !isPausedForLevelUp) {
        OpenLevelUpMenu();
    }

    // 後始末（nullptrや寿命切れの弾・敵・オーブを除去）
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

    //XPオーブ
    for (auto* o : xpOrbs) if (o) o->Draw();
    //敵
    for (auto* e : enemies) if (e) e->Draw();
    //弾
    for (auto* b : bullets) if (b) b->Draw();
    //Player
    player->Draw();

    if (isPausedForLevelUp) {
        RenderLevelUpOverlay();
    }
}


void Game::Shoot() {
    // 近接敵へ自動照準して発射
    // レベルでprojectilesPerShot(一度に撃つ弾の数)が増える
    float px = player->GetX();
    float py = player->GetY();
    const float speed = 260.0f; // 弾速
    const float life  = 1.3f;   // 秒

    if (projectilesPerShot >= 0) {
        // 敵がいれば最も近い敵に向けて撃つ、いなければ上方向
        float vx = 0.f, vy = speed;
        if (!enemies.empty()) {
            Enemy* nearest = nullptr;
            float bestD2 = 1e9f;
            for (auto* e : enemies) {
                //全Enemyの中で一番近いものをPickupしてnearestに格納
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
    // 画面外周から出現
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
    int hp = 2; // 基本HP
    enemies.push_back(new Enemy(x, y, spd, size, hp));
}

void Game::OpenLevelUpMenu() {
    isPausedForLevelUp    = true;
    needLevelUpKeyRelease = true;
    hoverChoice           = 0;

    RollLevelUpChoices(); // 選択肢抽選

    // タイトル/コンソールに今回の3候補を明示
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
        fireInterval = std::max(0.10f, fireInterval * levelup_fire_rate_multiplier);
        std::cout << "Applied: Fire Rate Up -> interval=" << fireInterval << "\n";
        break;
    case LUC_BULLET:
        projectilesPerShot += 1;
        std::cout << "Applied: +1 Bullet -> count=" << projectilesPerShot << "\n";
        break;
    case LUC_HEAL:
        playerHP = std::min(playerMaxHP, playerHP + levelup_heal_amount);
        std::cout << "Applied: Heal +" << levelup_heal_amount
                  << " -> HP " << playerHP << "/" << playerMaxHP << "\n";
        break;
    case LUC_SPEED:
        player->MultiplySpeed(1.15f);
        std::cout << "Applied: Move Speed +15% -> speed=" << player->GetSpeed() << "\n";
        break;
    default: break;
    }

    // キュー処理
    pendingLevelUps = std::max(0, pendingLevelUps - 1);
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

// 4候補から重複なしで3つ選ぶ（部分Fisher-Yates。<algorithm>不要）
void Game::RollLevelUpChoices() {
    int pool[4] = { LUC_FIRE_RATE, LUC_BULLET, LUC_HEAL, LUC_SPEED };
    for (int i = 0; i < 3; ++i) {
        int j = std::rand() % (4 - i);     // [0, 4-i)
        levelUpChoices[i] = pool[j];
        // 末尾側とスワップして引いた候補を山から除く
        std::swap(pool[j], pool[3 - i]);
    }
}

void Game::RenderLevelUpOverlay() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 背景
    glColor4f(0.f,0.f,0.f,0.5f);
    glBegin(GL_QUADS);
    glVertex2f(0,0); glVertex2f((float)winW,0);
    glVertex2f((float)winW,(float)winH); glVertex2f(0,(float)winH);
    glEnd();

    ComputeLevelUpLayout();

    auto drawBox = [&](int idx) {
        const Rect& r = levelUpBoxes[idx];
        int code = levelUpChoices[idx];

        // 面と枠
        glColor4f(0.15f,0.15f,0.15f,0.9f);
        glBegin(GL_QUADS);
        glVertex2f(r.x0,r.y0); glVertex2f(r.x1,r.y0);
        glVertex2f(r.x1,r.y1); glVertex2f(r.x0,r.y1);
        glEnd();

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

        // アイコン（中心）
        float cx = (r.x0 + r.x1) * 0.5f;
        float cy = (r.y0 + r.y1) * 0.5f;

        switch (code) {
            case LUC_FIRE_RATE: {
                glColor4f(0.8f,0.8f,0.2f,1.f);
                glBegin(GL_LINES);
                for (int i=0;i<5;++i){
                    float yy = cy - 30.f + i*15.f;
                    glVertex2f(cx - 60.f, yy); glVertex2f(cx + 60.f, yy);
                }
                glEnd();
                break;
            }
            case LUC_BULLET: {
                glColor4f(0.9f,0.9f,0.9f,1.f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(cx, cy);
                for (int i=0;i<=10;++i){
                    float a = i*3.1415926f/5.f;
                    float rr = (i%2==0)? 40.f : 18.f;
                    glVertex2f(cx + std::cos(a)*rr, cy + std::sin(a)*rr);
                }
                glEnd();
                break;
            }
            case LUC_HEAL: {
                glColor4f(0.9f,0.2f,0.2f,1.f);
                glBegin(GL_QUADS);
                glVertex2f(cx-15, cy-45); glVertex2f(cx+15, cy-45);
                glVertex2f(cx+15, cy+45); glVertex2f(cx-15, cy+45);
                glVertex2f(cx-45, cy-15); glVertex2f(cx+45, cy-15);
                glVertex2f(cx+45, cy+15); glVertex2f(cx-45, cy+15);
                glEnd();
                break;
            }
            case LUC_SPEED: {
                // 速度：斜め矢印チックな三角を複数
                glColor4f(0.2f,0.8f,0.2f,1.f);
                glBegin(GL_TRIANGLES);
                for (int k=0;k<3;++k){
                    float ox = cx - 50.f + k*35.f;
                    float oy = cy - 20.f + k*8.f;
                    glVertex2f(ox, oy);
                    glVertex2f(ox+35.f, oy+10.f);
                    glVertex2f(ox, oy+20.f);
                }
                glEnd();
                break;
            }
        }
    };

    drawBox(0); drawBox(1); drawBox(2);

    glDisable(GL_BLEND);
}


void Game::ComputeLevelUpLayout() {
    // RenderLevelUpOverlay と同じ配置を計算して矩形を更新
    float boxW = 180.f, boxH = 100.f, gap = 30.f;
    float cx = winW * 0.5f, cy = winH * 0.55f;

    auto setBox = [&](int idx, float xCenter, float yCenter) {
        float x0 = xCenter - boxW * 0.5f;
        float y0 = yCenter - boxH * 0.5f;
        float x1 = xCenter + boxW * 0.5f;
        float y1 = yCenter + boxH * 0.5f;
        levelUpBoxes[idx] = { x0, y0, x1, y1 };
    };

    setBox(0, cx - boxW - gap*0.5f, cy); // [1] 発射レート
    setBox(1, cx,                    cy); // [2] 弾数
    setBox(2, cx + boxW + gap*0.5f,  cy); // [3] 回復
}

bool Game::PointInRect(float x, float y, const Rect& r) {
    return (x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1);
}



