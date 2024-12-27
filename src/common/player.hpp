#pragma once
#include "raylib.h"
#include "raymath.h"
#include "constants.hpp"

enum PlayerConnection {
  None = 0,
  Disconnected = 1,
  PoorConnection = 2,
  Connected = 3
};

struct Player {
  bool active;
  Font font;
  const char* avatar;
  Vector2 position;
  Vector2 velocity;
  Vector2 draw_offset;
  float rotation;
  Color player_color;
  PlayerConnection connection_state;
};

Player AddPlayer(int i, Font font);

void UpdatePlayer(Player* player, float frametime);

void DrawPlayer(Player player);

bool Shoot();

Vector2 GetPlayerSpawnPosition(int i);