#include "Bullet.h"
#include <GLFW/glfw3.h>
#include <cmath>

Bullet::Bullet(float x_, float y_, float vx_, float vy_, float life, int dmg, float size)
    : x(x_), y(y_), vx(vx_), vy(vy_), lifetime(life), radius(size), damage(dmg), alive(true) {}

void Bullet::Update(float dt) {
    if (!alive) return;
    lifetime -= dt;
    if (lifetime <= 0.0f) { alive = false; return; }
    x += vx * dt;
    y += vy * dt;
}

void Bullet::Draw() const {
    if (!alive) return;
    glColor3f(0.9f, 0.9f, 0.9f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    const int SEG = 10;
    for (int i = 0; i <= SEG; ++i) {
        float ang = (float)i / SEG * 6.2831853f;
        glVertex2f(x + std::cos(ang) * radius, y + std::sin(ang) * radius);
    }
    glEnd();
}

bool Bullet::Hit(float ex, float ey, float eSize) const {
    if (!alive) return false;
    float dx = x - ex;
    float dy = y - ey;
    float r = radius + eSize;
    return (dx * dx + dy * dy) <= (r * r);
}


void Bullet::Kill() {
    alive = false;
}

