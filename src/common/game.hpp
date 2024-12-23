#include "raylib.h"
#include "raymath.h"
#include "gameManager.hpp"

struct Game {
  GameManager gameManager;

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

    EndDrawing();
  }
};
