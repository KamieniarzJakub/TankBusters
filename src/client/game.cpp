#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "lobbyManager.hpp"
#include <raylib.h>
#include <raymath.h>

struct Game {
  GameManager gameManager;
  LobbyManager lobbyManager;
  GraphicsManager graphicsManager;

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
      if (lobbyManager.UpdateLobbyStatus()) {
        gameManager.NewGame(lobbyManager.ready_players);
        for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
          gameManager.players[i].active =
              (lobbyManager.players[i] == PlayerInfo::READY);
        }
        lobbyManager.RestartLobby();
      }
      lobbyManager.UpdatePlayers();
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
      graphicsManager.DrawTitle(lobbyManager);
      graphicsManager.DrawLobbyPlayers(lobbyManager);
      graphicsManager.DrawReadyMessage();
      graphicsManager.DrawTimer(lobbyManager);
    }

    EndDrawing();
  }
};
