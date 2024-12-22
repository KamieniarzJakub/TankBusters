#pragma once
#include "raylib.h"

struct Bullet{
    bool active;
    Vector2 position;
    float rotation;
};

Bullet CreateBullet(Vector2 position, float rotation);

void UpdateBullet(Bullet *asteroid, float frametime);

void DrawBullet(Bullet bullet);