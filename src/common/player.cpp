#include "player.hpp"
#include "raylib.h"
#include "raymath.h"

void UpdatePlayer(Player* player)
{
  player->position = Vector2 {200, 100};
  player->rotation = 0;
  player->velocity = Vector2 {0, 0};

}

void DrawPlayer(Player player)
{
  DrawPoly(player.position, 3, 64, player.rotation, RED);
}