#pragma once
#include "constants.hpp"
#include <raylib.h>

struct Asteroid {
  bool active;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  float rotation_speed;
  int size;
  int polygon;
};

Vector2 GetRandomPosition();

Vector2 GetRandomVelocity(Vector2 position);

Asteroid
CreateAsteroid(Vector2 position = GetRandomPosition(),
               Vector2 velocity = {0, 0},
               int size = GetRandomValue(Constants::ASTEROID_SIZE_MIN,
                                         Constants::ASTEROID_SIZE_MAX));

void UpdateAsteroid(Asteroid *asteroid, float frametime);
