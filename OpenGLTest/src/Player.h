#pragma once

class Player {
public:
    Player(int windowWidth, int windowHeight);
    void Move(int dx, int dy); // -1 or 1
    void Update(float dt);
    void Draw();
    float GetX() const;
    float GetY() const;
    float GetSize() const;
    float GetSpeed() const;
    void  MultiplySpeed(float factor);
    void OnHit(int damage) {
        hitFlashT = 0.20f;      // 0.2秒間隔で赤フラッシュ
        invincibleT = 1.8f;    // 1.8秒無敵
    }
    bool IsInvincible() const { return invincibleT > 0.0f; }

private:
    float x, y;
    float speed;
    int winWidth, winHeight;
    float size;
    // 被弾時フラッシュ用
    float hitFlashT = 0.0f;
    float invincibleT = 0.0f;
};
