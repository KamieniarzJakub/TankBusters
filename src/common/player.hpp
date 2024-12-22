#pragma once
#include "raylib.h"

enum PlayerConnection {
  None = 0,
  Disconnected = 1,
  PoorConnection = 2,
  Connected = 3
};

struct Player {
  bool alive;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  Color player_color;
  PlayerConnection connection_state;
};

Player AddPlayer(int i);

void UpdatePlayer(Player* player);

void DrawPlayer(Player player, Texture texture);

bool Shoot();

Vector2 GetPlayerSpawnPosition(int i);