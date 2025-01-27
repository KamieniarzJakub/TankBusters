#include "constants.hpp"
#include "game.cpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <thread>

void read_config_file(const std::string path_relative, std::string &host,
                      std::string &port) {
  std::ifstream config(path_relative);
  if (!config.is_open()) {
    TraceLog(LOG_ERROR, "CONFIG: Couldn't open server.json file");
    exit(1);
  }

  json config_json = json::parse(config);
  config.close();

  try {
    host = config_json.at("host");
    port = config_json.at("port");
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR,
             "CONFIG: Expected field not found in server.json file: %s",
             ex.what());
    exit(1);
  }
}

int main() {
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(Constants::screenWidth, Constants::screenHeight,
             Constants::windowTitle.c_str());

  std::string host, port;
  read_config_file("resources/server.json", host, port);
  Game game = Game(host.c_str(), port.c_str());

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    game.updateDrawFrame();
  }

  // std::this_thread::sleep_for(1s);
  // destroy the window and cleanup the OpenGL context
  CloseWindow();
  return 0;
}
