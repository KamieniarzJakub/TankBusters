#pragma once
#include "raylib.h"
#include <chrono>
#include <nlohmann/json.hpp>

using namespace std::chrono;
using json = nlohmann::json;

struct Bullet {
  bool active;
  Vector2 position;
  float rotation;
};

Bullet CreateBullet(Vector2 position, float rotation);

void UpdateBullet(Bullet *bullet, duration<double> frametime);

void to_json(json &j, const Bullet &b);
void from_json(const json &j, Bullet &b);
