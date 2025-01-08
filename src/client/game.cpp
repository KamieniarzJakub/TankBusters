#include "gameManager.hpp"
#include "gameStatus.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"
#include <iostream>
#include <raylib.h>
#include <raymath.h>

struct Game {
  GameManager gameManager;
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;
  int selected_room = 0;
  unsigned int number_of_rooms;

  Game(const char *host, const char *port)
      : gameManager(GameManager()), graphicsManager(GraphicsManager()),
        networkManager(host, port) {
      number_of_rooms = networkManager.get_rooms().size();
  }

  void updateDrawFrame(void) {
    //MAYBE move somewhere else
    if (!networkManager.room_id)
    {
      if (IsKeyPressed(KEY_SPACE) && networkManager.get_rooms()[selected_room].players!=Constants::PLAYERS_MAX) {
        networkManager.room_id=selected_room+1; //TODO Server respond to request
      } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)){
        selected_room--;
        selected_room%=number_of_rooms;
      } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)){
        selected_room++;
        selected_room%=number_of_rooms;
      }
      //TraceLog(LOG_DEBUG, "SELECTED ROOM: %d", selected_room);
    } else{
      gameManager.UpdateGame();
      if (gameManager.ReturnToRooms()) networkManager.room_id = 0;
    }
    

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (!networkManager.room_id)
    {
      graphicsManager.DrawRoomTitle();
      graphicsManager.DrawRoomSubTitle();
      graphicsManager.DrawRooms(networkManager.get_rooms(), selected_room);
      graphicsManager.DrawRoomBottomText();
    } else {
      graphicsManager.DrawGame(gameManager);
    }

    EndDrawing();
  }
};
