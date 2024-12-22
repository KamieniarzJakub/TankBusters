#include "asteroid.hpp"
#include "constants.hpp"
#include "player.hpp"
#include "raylib.h"
#include "raymath.h"
#include "resource_dir.hpp" // utility header for SearchAndSetResourceDir
#include <vector>

struct Game {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  Texture players_textures[Constants::PLAYERS_MAX];

  float _spawnerTime;

  Game() {
    // Utility function from resource_dir.h to find the resources folder and set
    // it as the current working directory so we can load from it
    SearchAndSetResourceDir("resources");

    // Load a texture from the resources directory
    asteroids = std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid());
    players = std::vector<Player>(Constants::PLAYERS_MAX, Player());
    _spawnerTime = 0;
    for (int i=0; i<Constants::PLAYERS_MAX; i++)
    {
      players[i] = AddPlayer(i);
      players_textures[i] = LoadTexture("wabbit_alpha.png");
    }
  }

  ~Game() 
  {
    for(int i=0; i<Constants::PLAYERS_MAX; i++) UnloadTexture(players_textures[i]);
  }

  void updateDrawFrame(void) {
    float frametime = GetFrameTime();

    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
      UpdateAsteroid(&asteroids[i], frametime);
    }

    for (int i=0; i< Constants::PLAYERS_MAX; i++) {
      UpdatePlayer(&players[i]);
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
      DrawPlayer(players[i], players_textures[i]);
    }

    EndDrawing();
  }

  void AddAsteroid()
  {
    bool created = false;

    for (int i=0; i<Constants::ASTEROIDS_MAX; i++)
    {
      if (asteroids[i].active) continue;
      asteroids[i] = CreateAsteroid();
      created = true;
      break;
    }
    if (!created)
    {
      TraceLog(LOG_ERROR, "Failed to create an asteroid - no empty slots left");
    }
  }
};
