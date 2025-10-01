#pragma once

class Enemy {
public:
    Enemy(float x, float y, float speed, float size, int hp);
    void Update(float dt, float targetX, float targetY);
    void Draw() const;
    bool IsAlive() const { return hp > 0; }
    void Damage(int d) { hp -= d; }
    float X() const { return x; }
    float Y() const { return y; }
    float Size() const { return radius; }
    float DirX() const { return dirX; }  
    float DirY() const { return dirY; }

private:
    float x, y;
    float speed;
    float radius;
    int hp;
    float dirX = 1.0f, dirY = 0.0f;
};