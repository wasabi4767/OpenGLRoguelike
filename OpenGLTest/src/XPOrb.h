#pragma once

class XPOrb {
public:
    XPOrb(float x, float y, int amount, float size = 5.0f)
        : x(x), y(y), xp(amount), radius(size), alive(true) {}
    void Draw() const;
    bool CheckPickup(float px, float py, float pickupR);
    bool IsAlive() const { return alive; }
    int Amount() const { return xp; }

private:
    float x, y;
    int xp;
    float radius;
    bool alive;
};