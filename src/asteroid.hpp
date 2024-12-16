#pragma once
#include "raylib.h"

struct Asteroid {
  Vector2 position;
  Vector2 velocity;
  double rotation;
  double rotation_speed;
  int size;
  bool active;
};
