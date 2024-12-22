#include "asteroid.hpp"
#include "constants.hpp"
#include "raymath.h"
#include "spaceJunkCollector.hpp"

Asteroid CreateAsteroid() {
  Vector2 position = GetRandomPosition();
  return (Asteroid){.active = true,
                    .position = position,
                    .velocity = GetRandomVelocity(position),
                    .rotation = (float)GetRandomValue(0, 360),
                    .rotation_speed = (float)GetRandomValue(-Constants::ASTEROID_ROTATION_SPEED_MAX, Constants::ASTEROID_ROTATION_SPEED_MAX),
                    .size = GetRandomValue(Constants::ASTEROID_SIZE_MIN, Constants::ASTEROID_SIZE_MAX),
                    .polygon = GetRandomValue(Constants::ASTEROID_POLYGON_MIN, Constants::ASTEROID_POLYGON_MAX)};
}

void UpdateAsteroid(Asteroid *asteroid, float frametime) {
  if (!asteroid->active)
    return;

  if (SpaceJunkCollector(asteroid->position))
  {
    asteroid->active = false;
    return;
  }

  asteroid->position = Vector2Add(asteroid->position,
                                  Vector2Scale(asteroid->velocity, frametime));
  asteroid->rotation += asteroid->rotation_speed * frametime;
}

void DrawAsteroid(Asteroid asteroid) {
  DrawPolyLines(asteroid.position, asteroid.polygon, asteroid.size, asteroid.rotation, WHITE);
}

Vector2 GetRandomPosition()
{
  int side = GetRandomValue(-1,2);
  if (side%2)
    return Vector2 {float(side)*Constants::ASTEROID_SIZE_MAX+float(side+1)/2.0f*Constants::screenWidth, (float)GetRandomValue(0,Constants::screenHeight)};
  return Vector2 {(float)GetRandomValue(0,Constants::screenWidth), float(side-1)*Constants::ASTEROID_SIZE_MAX+float(side)/2.0f*Constants::screenHeight};
}

Vector2 GetRandomVelocity(Vector2 position)
{
  Vector2 velocity = Vector2Subtract({Constants::screenWidth/2, Constants::screenHeight/2}, position);
  velocity = Vector2Scale(Vector2Normalize(velocity), GetRandomValue(Constants::ASTEROID_SPEED_MIN, Constants::ASTEROID_SPEED_MAX));
  return Vector2Rotate(velocity, GetRandomValue(-Constants::ASTEROID_PATH_RANDOM_ANGLE, Constants::ASTEROID_PATH_RANDOM_ANGLE));
}

