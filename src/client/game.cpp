#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"
#include <raylib.h>
#include <raymath.h>

struct Game {
  GameManager gameManager;
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;

  Game(const char *host, const char *port)
      : gameManager(GameManager()), graphicsManager(GraphicsManager()),
        networkManager(host, port) {}

  void updateDrawFrame(void) {
    gameManager.UpdateGameStatus();
    // TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
    if (gameManager.status == Status::GAME) {
      float frametime = GetFrameTime();

      gameManager.ManageCollisions();

      gameManager.UpdatePlayers(frametime);
      gameManager.UpdateBullets(frametime);
      gameManager.UpdateAsteroids(frametime);

      gameManager.AsteroidSpawner(GetTime());
    } else {
      if (gameManager.UpdateLobbyStatus()) {
        gameManager.NewGame(gameManager.GetReadyPlayers());
        gameManager.RestartLobby();
      }
      gameManager.UpdatePlayersLobby();
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (gameManager.status == Status::GAME ||
        gameManager.status == Status::END_OF_ROUND) {
      graphicsManager.DrawAsteroids(gameManager);
      graphicsManager.DrawPlayers(gameManager);
      graphicsManager.DrawBullets(gameManager);
      if (gameManager.status == Status::END_OF_ROUND) {
        graphicsManager.DrawWinnerText(gameManager);
        graphicsManager.DrawNewRoundCountdown(gameManager);
      }
      graphicsManager.DrawTime(gameManager, GetTime());
    } else {
      graphicsManager.DrawTitle(gameManager);
      graphicsManager.DrawLobbyPlayers(gameManager);
      graphicsManager.DrawReadyMessage();
      graphicsManager.DrawTimer(gameManager);
    }

    EndDrawing();
  }
};
