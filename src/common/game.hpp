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

    for (int i=0; i< Constants::PLAYERS_MAX; i++) {
      UpdatePlayer(&players[i]);
      if (Shoot())
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
      DrawAsteroid(asteroids[i]);
    }
    for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
      DrawPlayer(players[i], player_texture);
    }

    for (int i = 0; i < Constants::PLAYERS_MAX*Constants::BULLETS_PER_PLAYER; i++) {
      DrawBullet(bullets[i]);
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

  void AddBullet(Player player, int player_number)
  {
    for (int i=player_number * Constants::BULLETS_PER_PLAYER; i<(player_number+1) * Constants::BULLETS_PER_PLAYER; i++)
    {
      if (bullets[i].active) continue;
      Vector2 offset = Vector2Rotate(Vector2 {float(player_texture.width)/2.0f*Constants::PLAYER_TEXTURE_SCALE, 0.0f}, player.rotation*DEG2RAD);
      bullets[i] = CreateBullet(Vector2Add(player.position, offset), player.rotation);
      return;
    }
    TraceLog(LOG_INFO, "Faild to create a bullet for player[%d] - no bullets left", player_number);
  }
};
