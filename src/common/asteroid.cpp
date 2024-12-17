#include "asteroid.hpp"
#include "constants.hpp"
#include "raymath.h"

Asteroid CreateAsteroid(Vector2 position, Vector2 velocity) {
  return (Asteroid){.active = true,
                    .position = position,
                    .velocity = velocity,
                    .rotation = GetRandomValue(0, 360),
                    .rotation_speed =
                        GetRandomValue(Constants::ASTEROID_ROTATION_SPEED_MIN,
                                       Constants::ASTEROID_ROTATION_SPEED_MAX)};
}

void UpdateAsteroid(Asteroid *asteroid, float frametime) {
  if (!asteroid->active)
    return;

  asteroid->position = Vector2Add(asteroid->position,
                                  Vector2Scale(asteroid->velocity, frametime));
  asteroid->rotation += asteroid->rotation * frametime;
}

void DrawAsteroid(Asteroid asteroid) {
  DrawPolyLines(asteroid.position, 3, 64, asteroid.rotation, WHITE);
}
