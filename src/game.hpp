#include "asteroid.hpp"
#include "constants.hpp"
#include "player.hpp"
#include "raylib.h"
#include "resource_dir.hpp" // utility header for SearchAndSetResourceDir
#include <vector>

struct Game {
  Texture asteroid_texture;
  Texture player_texture;

  std::vector<Asteroid> asteroids;
  std::vector<Player> players;

  Game() {
    // Utility function from resource_dir.h to find the resources folder and set
    // it as the current working directory so we can load from it
    SearchAndSetResourceDir("resources");

    // Load a texture from the resources directory
    player_texture = LoadTexture("wabbit_alpha.png");
    asteroid_texture = LoadTexture("wabbit_alpha.png");
    asteroids = std::vector<Asteroid>(20, Asteroid());
    players = std::vector<Player>(4, Player());
  }

  ~Game() {
    UnloadTexture(asteroid_texture);
    UnloadTexture(player_texture);
  }

  void updateDrawFrame(void) {
    float frametime = GetFrameTime();

    for (int i = 0; i < asteroids.size(); i++) {
      UpdateAsteroid(&asteroids[i], frametime);
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    for (int i = 0; i < asteroids.size(); i++) {
      DrawAsteroid(asteroids[i]);
    }

    EndDrawing();
  }
};
