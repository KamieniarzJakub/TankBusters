#include "raylib.h"
#include "raymath.h"
#include "gameManager.hpp"

struct Game {
  GameManager gameManager;

  void updateDrawFrame(void) 
  {
    if (gameManager.GetGameStatus()==Status::GAME)
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

    gameManager.DrawAsteroids();
    gameManager.DrawPlayers();
    gameManager.DrawBullets();
    gameManager.DrawTime(GetTime());
    if(gameManager.status)
    {
      
    }

    EndDrawing();
  }
};
