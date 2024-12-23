#pragma once
#include "raylib.h"

enum PlayerConnection {
  None = 0,
  Disconnected = 1,
  PoorConnection = 2,
  Connected = 3
};

struct Player {
  bool active;
  Texture texture;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  Color player_color;
  PlayerConnection connection_state;
};

Player AddPlayer(int i, Texture texture);

void UpdatePlayer(Player* player, float frametime);

void DrawPlayer(Player player, Font player_font);

bool Shoot();

Vector2 GetPlayerSpawnPosition(int i);