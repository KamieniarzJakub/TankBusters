#pragma once
#include "raylib.h"
#include "raymath.h"
#include "constants.hpp"
#include "spaceJunkCollector.hpp"

struct Bullet{
    bool active;
    Vector2 position;
    float rotation;
};

Bullet CreateBullet(Vector2 position, float rotation);

void UpdateBullet(Bullet *asteroid, float frametime);

void DrawBullet(Bullet bullet);