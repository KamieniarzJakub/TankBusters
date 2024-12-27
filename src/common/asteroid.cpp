#include "asteroid.hpp"

Asteroid CreateAsteroid(Vector2 position, Vector2 velocity, int size) {
  velocity = Vector2Length(velocity) == 0 ? GetRandomVelocity(position) : velocity; 
  Asteroid asteroid;
  asteroid.active = true;
  asteroid.position = position;
  asteroid.velocity = velocity;
  asteroid.rotation = (float)GetRandomValue(0, 360);
  asteroid.rotation_speed = (float)GetRandomValue(-Constants::ASTEROID_ROTATION_SPEED_MAX, Constants::ASTEROID_ROTATION_SPEED_MAX);
  asteroid.size = size;
  asteroid.polygon = GetRandomValue(Constants::ASTEROID_POLYGON_MIN, Constants::ASTEROID_POLYGON_MAX);
  return asteroid;
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

