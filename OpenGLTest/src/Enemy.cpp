#include "Enemy.h"
#include "Shaders/EnemyShader.h"
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
        dirX = dx; dirY = dy;

        x += dx * speed * dt;
        y += dy * speed * dt;
    }
}

void Enemy::Draw() const {
    if (!EnemyShader::IsSupported()) {
        // フォールバック用（今までの四角）
        glColor3f(0.8f, 0.2f, 0.2f);
        glBegin(GL_QUADS);
        glVertex2f(x - radius, y - radius);
        glVertex2f(x + radius, y - radius);
        glVertex2f(x + radius, y + radius);
        glVertex2f(x - radius, y + radius);
        glEnd();
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    EnemyShader::Use();

    float t = (float)glfwGetTime();

    // プレイヤー方向を向かせる
    EnemyShader::SetUniforms(
        x, y,                  // center
        radius,                // size（半径ベース）
        t,                     // time
        dirX, dirY,            // 先端の向き
        1.00f, 0.35f, 0.25f,   // colA（内側：サーモン）
        1.00f, 0.10f, 0.55f,   // colB（縁：マゼンタ寄り）
        1.05f                  // intensity
    );

    float R = radius * 2.0f + 8.0f; // 被覆クアッド
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x - R, y - R);
    glVertex2f(x + R, y - R);
    glVertex2f(x - R, y + R);
    glVertex2f(x + R, y + R);
    glEnd();

    EnemyShader::Stop();

    glDisable(GL_BLEND);
}

