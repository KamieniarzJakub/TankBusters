#include "constants.hpp"
#include "game.cpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <raylib.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

int main() {
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(Constants::screenWidth, Constants::screenHeight,
             Constants::windowTitle.c_str());

  std::ifstream config("resources/server.json");
  if (!config.is_open()) {
    TraceLog(LOG_ERROR, "CONFIG: Couldn't open server.json file");
    exit(1);
  }

  json config_json = json::parse(config);
  config.close();

  std::string host, port;
  try {
    host = config_json.at("host");
    port = config_json.at("port");
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR,
             "CONFIG: Expected field not found in server.json file: %s",
             ex.what());
    exit(1);
  }

  // FIXME: initalize network connection after displaying GUI
  Game game = Game(host.c_str(), port.c_str());

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(game.updateDrawFrame, 0, 1);
#else
  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    game.updateDrawFrame();
  }
#endif

  // destroy the window and cleanup the OpenGL context
  CloseWindow();
  return 0;
}
