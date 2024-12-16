#include "asteroid.hpp"
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
    BeginDrawing();

    ClearBackground(BLACK);

    DrawText("Congrats! You created your first window!", 190, 140, 20,
             LIGHTGRAY);

    // draw some text using the default font
    DrawText("Hello Raylib", 200, 600, 20, WHITE);

    // draw our texture to the screen
    DrawTexture(player_texture, 400, 200, WHITE);

    EndDrawing();
  }
};
