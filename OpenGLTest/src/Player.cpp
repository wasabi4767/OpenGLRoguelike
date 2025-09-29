#include "Player.h"
#include "Shaders/PlayerFlareShader.h"
#include <GLFW/glfw3.h>
#include <algorithm>

Player::Player(int windowWidth, int windowHeight)
    : x(windowWidth / 2.0f), y(100), speed(50.0f), winWidth(windowWidth), winHeight(windowHeight), size(10.0f) {}

void Player::Move(int dx, int dy) {
    x += dx * speed * 0.0008f;
    y += dy * speed * 0.0008f;

    if (x < size) x = size;
    if (x > winWidth - size) x = winWidth - size;
    if (y < size) y = size;
    if (y > winHeight - size) y = winHeight - size;
}

void Player::Update(float dt) {
    // 無敵・フラッシュのタイマー減衰
    if (hitFlashT   > 0.0f) hitFlashT   = std::max(0.0f, hitFlashT   - dt);
    if (invincibleT > 0.0f) invincibleT = std::max(0.0f, invincibleT - dt);
}

void Player::Draw() {
    float cx = x, cy = y;
    float inner = size;
    float outer = size * 3.0f;
    float t = (float)glfwGetTime();

    // シェーダに渡す値を計算
    float hitAmt = (hitFlashT > 0.0f) ? (hitFlashT / 0.20f) : 0.0f; // 0..1
    float invOn  = (invincibleT > 0.0f) ? 1.0f : 0.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    if (PlayerFlareShader::IsSupported()) {
        PlayerFlareShader::Use();
        PlayerFlareShader::SetUniforms(
            cx, cy, inner, outer, t,
            0.20f, 0.60f, 1.00f,   // colA
            0.20f, 1.00f, 0.60f,   // colB
            1.2f,                  // intensity
            hitAmt,                
            invOn                  
        );
        float R = outer + 10.0f;
        glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(cx - R, cy - R);
        glVertex2f(cx + R, cy - R);
        glVertex2f(cx - R, cy + R);
        glVertex2f(cx + R, cy + R);
        glEnd();
        PlayerFlareShader::Stop();
    }

    glDisable(GL_BLEND);
}



float Player::GetX() const { return x; }
float Player::GetY() const { return y; }
float Player::GetSize() const { return size; }
float Player::GetSpeed() const { return speed; }
void  Player::MultiplySpeed(float factor) { speed *= factor; }

