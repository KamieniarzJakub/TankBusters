#include "gameManager.hpp"
#include "gameStatus.hpp"
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
        networkManager(host, port) {
    std::vector<Room> rooms =
        networkManager.get_rooms(); // FIXME: move somewhere else
    TraceLog(LOG_INFO, ("ROOMS: " + json(rooms).dump()).c_str());
  }

  void updateDrawFrame(void) {
    gameManager.UpdateGameStatus();
    // TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
    if (gameManager.status == GameStatus::GAME) {
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

    if (gameManager.status == GameStatus::GAME ||
        gameManager.status == GameStatus::END_OF_ROUND) {
      graphicsManager.DrawAsteroids(gameManager);
      graphicsManager.DrawPlayers(gameManager);
      graphicsManager.DrawBullets(gameManager);
      if (gameManager.status == GameStatus::END_OF_ROUND) {
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
