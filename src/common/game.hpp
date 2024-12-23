#include "asteroid.hpp"
#include "constants.hpp"
#include "player.hpp"
#include "bullet.hpp"
#include "raylib.h"
#include "raymath.h"
#include "resource_dir.hpp" // utility header for SearchAndSetResourceDir
#include <vector>

struct Game {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  std::vector<Bullet> bullets; 
  Texture player_texture;

  float _spawnerTime;

  Game() {
    // Utility function from resource_dir.h to find the resources folder and set
    // it as the current working directory so we can load from it
    SearchAndSetResourceDir("resources");

    // Load a texture from the resources directory

    //Zakładam, że liczba asteroid, graczy i pocisków jest ograniczona, każdy obiekt (struktura) posiada właściwość 'active'
    //Jeżeli chodzi o dane, które chcemy przesyłać, to można wysyłać cały vector
    asteroids = std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid());
    players = std::vector<Player>(Constants::PLAYERS_MAX, Player());
    bullets = std::vector<Bullet>( Constants::BULLETS_PER_PLAYER*Constants::PLAYERS_MAX, Bullet());
    _spawnerTime = 0;
    for (int i=0; i<Constants::PLAYERS_MAX; i++)
    {
      players[i] = AddPlayer(i);
      player_texture = LoadTexture("Player.png");
    }
  }

  ~Game() 
  {
    UnloadTexture(player_texture);
  }

  void updateDrawFrame(void) {
    float frametime = GetFrameTime();

    //Wyszło mega spaghetti - działa, ale do przepisania
    //Colision Asteroids-Players-Bullets
    for (int i=0; i< Constants::ASTEROIDS_MAX; i++) 
    {
      if (!asteroids[i].active) continue;
      for (int j=0; j<Constants::PLAYERS_MAX; j++)
      {
        for (int k=j*Constants::BULLETS_PER_PLAYER; k<(j+1)*Constants::BULLETS_PER_PLAYER; k++)
        {
          if(!bullets[k].active) continue;
          if(CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE, asteroids[i].position, asteroids[i].size))
          {
            bullets[k].active = false;
            asteroids[i].active = false;
            SplitAsteroid(asteroids[i].position, asteroids[i].velocity, float(asteroids[i].size)/Constants::ASTEROID_SPLIT_LOSS);
            break;
          }
          for (int l=0; l<Constants::PLAYERS_MAX; l++)
          {
            if(j==l || !players[l].active) continue;
            if (CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE, players[l].position, float(player_texture.width)*Constants::PLAYER_TEXTURE_SCALE/2.0f))
            {
              players[l].active = false;
              bullets[k].active = false;
              break;
            }
          }
        }
        if (!players[j].active) continue;
        if(CheckCollisionCircles(players[j].position, float(player_texture.width)*Constants::PLAYER_TEXTURE_SCALE/2.0f, asteroids[i].position, asteroids[i].size))
        {
          players[j].active = false;
          asteroids[i].active = false;
        }
      }
    }
    for (int i =0; i<Constants::PLAYERS_MAX; i++){
      UpdatePlayer(&players[i], frametime);
      if (players[i].active && Shoot())
      {
        AddBullet(players[i], i);
      }
    }

    for (int i = 0; i < Constants::PLAYERS_MAX*Constants::BULLETS_PER_PLAYER; i++) {
      UpdateBullet(&bullets[i], frametime);
    }

    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
      UpdateAsteroid(&asteroids[i], frametime);
    }

    if (GetTime()>_spawnerTime+Constants::ASTEROID_SPAWN_DELAY)
    {
      _spawnerTime = GetTime();
      AddAsteroid();
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
      if (asteroids[i].active)
      {
        //DrawCircle(asteroids[i].position.x, asteroids[i].position.y, asteroids[i].size, PINK);
        DrawAsteroid(asteroids[i]);
      }
      
    }
    for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
      //DrawCircle(players[i].position.x, players[i].position.y, float(player_texture.width)*Constants::PLAYER_TEXTURE_SCALE/2.0f, PURPLE);
      DrawPlayer(players[i], player_texture);
    }

    for (int i = 0; i < Constants::PLAYERS_MAX*Constants::BULLETS_PER_PLAYER; i++) {
      if (bullets[i].active)
      {
        DrawBullet(bullets[i]);
      }
    }

    EndDrawing();
  }

  void AddAsteroid()
  {
    for (int i=0; i<Constants::ASTEROIDS_MAX; i++)
    {
      if (asteroids[i].active) continue;
      asteroids[i] = CreateAsteroid();
      return;
    }
      TraceLog(LOG_ERROR, "Failed to create an asteroid - no empty slots left");
  }

  void SplitAsteroid(Vector2 position, Vector2 velocity, int size)
  {
    if (size<Constants::ASTEROID_SIZE_MIN) return;
    int toSpawn = 2;
    for (int i=0; i<Constants::ASTEROIDS_MAX; i++)
    {
      if (!toSpawn) return;
      if (asteroids[i].active) continue;
      Vector2 new_velocity = Vector2Rotate(velocity, toSpawn%2 ? GetRandomValue(60,120) : GetRandomValue(-120, -60));
      asteroids[i] = CreateAsteroid(position, new_velocity, size);
      toSpawn--;
    }
      TraceLog(LOG_ERROR, "Failed to split an asteroid - no empty slots left");
  }

  void AddBullet(Player player, int player_number)
  {
    for (int i=player_number * Constants::BULLETS_PER_PLAYER; i<(player_number+1) * Constants::BULLETS_PER_PLAYER; i++)
    {
      if (bullets[i].active) continue;
      Vector2 offset = Vector2Rotate(Vector2 {float(player_texture.width)/2.0f*Constants::PLAYER_TEXTURE_SCALE, 0.0f}, player.rotation*DEG2RAD);
      bullets[i] = CreateBullet(Vector2Add(player.position, offset), player.rotation);
      return;
    }
    TraceLog(LOG_INFO, "Faild to shoot a bullet - player[%d]: no bullets left", player_number);
  }
};
