#include "player.hpp"
#include "raylib.h"
#include "raymath.h"
#include "constants.hpp"

Player AddPlayer(int i) {
  return (Player){  .active = true,
                    .position = GetPlayerSpawnPosition(i),
                    .velocity = {0,0},
                    .rotation = (float)GetRandomValue(0, 360),
                    .player_color = Constants::PLAYER_COLORS[i]};
}

void UpdatePlayer(Player* player, float frametime)
{
  int rot = (int)(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) - (int)(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
  player->rotation += rot * Constants::PLAYER_ROTATION_SPEED *  frametime;

  
  if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
  {
      Vector2 direction = { cosf(DEG2RAD * player->rotation), sinf(DEG2RAD * player->rotation) };
      player->velocity = Vector2Add(player->velocity, Vector2Scale(direction, Constants::PLAYER_ACCELERATION * frametime));
  }

  // Apply damping to velocity
  player->velocity = Vector2Scale(player->velocity, (1-Constants::PLAYER_DRAG/1000.0f));

  // Update position
  player->position = Vector2Add(player->position, Vector2Scale(player->velocity, frametime));

  // Keep player on the screen (wrap around)
  if (player->position.x < 0) player->position.x += Constants::screenWidth;
  if (player->position.x > Constants::screenWidth) player->position.x -= Constants::screenWidth;
  if (player->position.y < 0) player->position.y += Constants::screenHeight;
  if (player->position.y > Constants::screenHeight) player->position.y -= Constants::screenHeight;
}


void DrawPlayer(Player player, Texture texture)
{
  //DrawPoly(player.position, 3, 16, player.rotation, RED);
  //https://tradam.itch.io/raylib-drawtexturepro-interactive-demo
  Rectangle source = {0, 0, float(texture.width), float(texture.height)};
  Rectangle dest = {player.position.x, player.position.y, source.width*Constants::PLAYER_TEXTURE_SCALE, source.height*Constants::PLAYER_TEXTURE_SCALE};
  Vector2 origin = {dest.width/2.0f, dest.height/2.0f};
  if (!player.active) player.player_color.a = 25;
  DrawTexturePro(texture, source, dest, origin, player.rotation, player.player_color);
}

bool Shoot()
{
  if (IsKeyPressed(KEY_SPACE)) return true;
  return false;
}

Vector2 GetPlayerSpawnPosition(int i)
{
  return Vector2 {Constants::screenWidth/2, (float)i*Constants::screenHeight/4};
}