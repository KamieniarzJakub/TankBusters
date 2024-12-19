#pragma once
#include "raylib.h"

enum PlayerConnection {
  None = 0,
  Disconnected = 1,
  PoorConnection = 2,
  Connected = 3
};

struct Player {
  Vector2 position;
  Vector2 velocity;
  float rotation;
  PlayerConnection connection_state;
};

void UpdatePlayer(Player* player);

void DrawPlayer(Player player);