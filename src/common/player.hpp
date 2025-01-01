#pragma once
#include <raylib.h>
#include <raymath.h>

enum PlayerConnection {
  None = 0,
  Disconnected = 1,
  PoorConnection = 2,
  Connected = 3
};

struct Player {
  bool active;
  const char *avatar;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  Color player_color;
  PlayerConnection connection_state;
};

Player AddPlayer(int i);

void UpdatePlayer(Player *player, float frametime);

bool Shoot();

Vector2 GetPlayerSpawnPosition(int i);
