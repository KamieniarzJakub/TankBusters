#pragma once
#include "constants.hpp"
#include <nlohmann/json.hpp>
#include <raylib.h>

using json = nlohmann::json;

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

void UpdateAsteroid(Asteroid &asteroid, duration<double> frametime);

void to_json(json &j, const Asteroid &a);
void from_json(const json &j, Asteroid &a);
