#include "asteroid.hpp"
#include "spaceJunkCollector.hpp"
#include <raymath.h>
#include <vec2json.hpp>

Asteroid CreateAsteroid(Vector2 position, Vector2 velocity, int size) {
  velocity =
      Vector2Length(velocity) == 0 ? GetRandomVelocity(position) : velocity;
  Asteroid asteroid;
  asteroid.active = true;
  asteroid.position = position;
  asteroid.velocity = velocity;
  asteroid.rotation = (float)GetRandomValue(0, 360);
  asteroid.rotation_speed =
      (float)GetRandomValue(-Constants::ASTEROID_ROTATION_SPEED_MAX,
                            Constants::ASTEROID_ROTATION_SPEED_MAX);
  asteroid.size = size;
  asteroid.polygon = GetRandomValue(Constants::ASTEROID_POLYGON_MIN,
                                    Constants::ASTEROID_POLYGON_MAX);
  return asteroid;
}

void UpdateAsteroid(Asteroid &asteroid, duration<double> frametime) {
  if (!asteroid.active)
    return;

  if (SpaceJunkCollector(asteroid.position)) {
    asteroid.active = false;
    return;
  }

  asteroid.position = Vector2Add(
      asteroid.position, Vector2Scale(asteroid.velocity, frametime.count()));
  asteroid.rotation += asteroid.rotation_speed * frametime.count();
}

Vector2 GetRandomPosition() {
  int side = GetRandomValue(-1, 2);
  if (side % 2)
    return Vector2{float(side) * Constants::ASTEROID_SIZE_MAX +
                       float(side + 1) / 2.0f * Constants::screenWidth,
                   (float)GetRandomValue(0, Constants::screenHeight)};
  return Vector2{(float)GetRandomValue(0, Constants::screenWidth),
                 float(side - 1) * Constants::ASTEROID_SIZE_MAX +
                     float(side) / 2.0f * Constants::screenHeight};
}

Vector2 GetRandomVelocity(Vector2 position) {
  Vector2 velocity = Vector2Subtract(
      {(float)Constants::screenWidth / 2, (float)Constants::screenHeight / 2},
      position);
  velocity = Vector2Scale(Vector2Normalize(velocity),
                          GetRandomValue(Constants::ASTEROID_SPEED_MIN,
                                         Constants::ASTEROID_SPEED_MAX));
  return Vector2Rotate(velocity,
                       GetRandomValue(-Constants::ASTEROID_PATH_RANDOM_ANGLE,
                                      Constants::ASTEROID_PATH_RANDOM_ANGLE));
}

void to_json(json &j, const Asteroid &a) {
  j = json{{"active", a.active},
           {"position", a.position},
           {"velocity", a.velocity},
           {"rotation", a.rotation},
           {"rotation_speed", a.rotation_speed},
           {"size", a.size},
           {"polygon", a.polygon}};
}
void from_json(const json &j, Asteroid &a) {
  j.at("active").get_to(a.active);
  j.at("position").get_to(a.position);
  j.at("velocity").get_to(a.velocity);
  j.at("rotation").get_to(a.rotation);
  j.at("rotation_speed").get_to(a.rotation_speed);
  j.at("size").get_to(a.size);
  j.at("polygon").get_to(a.polygon);
}
