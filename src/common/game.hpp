#include "raylib.h"
#include "raymath.h"
#include "gameManager.hpp"
#include "lobbyManager.hpp"

struct Game {
  GameManager gameManager;
  LobbyManager lobbyManager;

  void updateDrawFrame(void) 
  {
    gameManager.UpdateGameStatus();
    //TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
    if (gameManager.status==Status::GAME)
    {
      float frametime = GetFrameTime();

      gameManager.ManageCollisions();

      gameManager.UpdatePlayers(frametime);
      gameManager.UpdateBullets(frametime);
      gameManager.UpdateAsteroids(frametime);
    
      gameManager.AsteroidSpawner(GetTime());
    }
    else
    {
      if (lobbyManager.UpdateLobbyStatus())
      {
        gameManager.NewGame(lobbyManager.ready_players);
        for (int i=0; i<Constants::PLAYERS_MAX; i++)
        {
          gameManager.players[i].active = (lobbyManager.players[i]==PlayerInfo::READY);
        }
        lobbyManager.RestartLobby();
      }
      lobbyManager.UpdatePlayers();
    }
    
    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if(gameManager.status==Status::GAME || gameManager.status==Status::END_OF_ROUND)
    {
      gameManager.DrawAsteroids();
      gameManager.DrawPlayers();
      gameManager.DrawBullets();
      if (gameManager.status==Status::END_OF_ROUND)
      {
        gameManager.DrawWinnerText();
        gameManager.DrawNewRoundCountdown();
      }
      gameManager.DrawTime(GetTime());
    }
    else
    {
      lobbyManager.DrawTitle(gameManager.win_font);
      lobbyManager.DrawLobbyPlayers(gameManager.font);
      lobbyManager.DrawReadyMessage(gameManager.font);
      lobbyManager.DrawTimer(gameManager.font);
    }
    
    EndDrawing();
  }
};
