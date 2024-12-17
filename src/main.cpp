#include "constants.hpp"
#include "game.hpp"
#include "raylib.h"
#include <memory>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif


int main() {
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(Constants::screenWidth, Constants::screenHeight,
             Constants::windowTitle.c_str());
  std::unique_ptr<Game> game = std::make_unique<Game>();

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(game->updateDrawFrame, 0, 1);
#else
  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    game->updateDrawFrame();
  }
#endif

  // destroy the window and cleanup the OpenGL context
  CloseWindow();
  return 0;
}
