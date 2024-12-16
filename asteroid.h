#ifndef ASTEROID_H_
#define ASTEROID_H_

#include "raylib.h"
#include "raymath.h"

#define ASTEROID_ROTATION_SPEED_MIN 100
#define ASTEROID_ROTATION_SPEED_MAX 250

typedef struct Asteroid
{
    bool active;
    Vector2 position;
    float rotation;
    float rotationSpeed;
    Vector2 velocity;
} Asteroid;

Asteroid CreateAsteroid(Vector2 position, Vector2 velocity);

void UpdateAsteroid(Asteroid* asteroid, float frametime);

void DrawAsteroid(Asteroid asteroid);

#endif