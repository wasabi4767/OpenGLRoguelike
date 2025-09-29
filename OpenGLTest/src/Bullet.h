#pragma once

class Bullet {
public:
    Bullet(float x, float y, float vx, float vy, float life, int dmg, float size = 4.0f);
    void Update(float dt);
    void Draw() const;
    bool IsAlive() const { return alive; }
    bool Hit(float ex, float ey, float eSize) const;
    int Damage() const { return damage; }
    void Kill();

    float X() const { return x; }
    float Y() const { return y; }
    float Size() const { return radius; }

private:
    float x, y;
    float vx, vy;
    float lifetime;
    float radius;
    int damage;
    bool alive;
};
