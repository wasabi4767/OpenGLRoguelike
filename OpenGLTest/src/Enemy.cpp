#include "Enemy.h"
#include <GLFW/glfw3.h>
#include <cmath>

Enemy::Enemy(float x_, float y_, float speed_, float size_, int hp_)
    : x(x_), y(y_), speed(speed_), radius(size_), hp(hp_) {}

void Enemy::Update(float dt, float tx, float ty) {
    float dx = tx - x;
    float dy = ty - y;
    float len = std::sqrt(dx*dx + dy*dy);
    if (len > 0.0001f) {
        dx /= len; dy /= len;
        x += dx * speed * dt;
        y += dy * speed * dt;
    }
}

void Enemy::Draw() const {
    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x - radius, y - radius);
    glVertex2f(x + radius, y - radius);
    glVertex2f(x + radius, y + radius);
    glVertex2f(x - radius, y + radius);
    glEnd();
}

