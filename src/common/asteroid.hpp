#pragma once
#include "raylib.h"

struct Asteroid {
  bool active;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  float rotation_speed;
  int size;
  int polygon;
};

Asteroid CreateAsteroid();

void UpdateAsteroid(Asteroid *asteroid, float frametime);

void DrawAsteroid(Asteroid asteroid);

Vector2 GetRandomPosition();

Vector2 GetRandomVelocity(Vector2 position);

