#pragma once
#include "raylib.h"

struct Asteroid {
  bool active;
  Vector2 position;
  Vector2 velocity;
  int rotation;
  int rotation_speed;
};

Asteroid CreateAsteroid(Vector2 position, Vector2 velocity);

void UpdateAsteroid(Asteroid *asteroid, float frametime);

void DrawAsteroid(Asteroid asteroid);
