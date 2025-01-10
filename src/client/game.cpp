#include <raylib.h>
#include <raymath.h>

#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"

struct Game {
  GameManager gameManager;
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;
  int selected_room = 0;
  unsigned int number_of_rooms;

  Game(const char *host, const char *port)
      : gameManager(GameManager()),
        graphicsManager(GraphicsManager()),
        networkManager(host, port) {
    std::vector<Room> rooms;
    bool status =
        networkManager.get_rooms(rooms);  // FIXME: move somewhere else
    if (status) {
      TraceLog(LOG_INFO, ("ROOMS: " + json(rooms).dump()).c_str());
    } else {
      TraceLog(LOG_ERROR, "NET: connection problem");
    }
  }

  void updateDrawFrame(void) {
    // MAYBE move somewhere else
    if (!networkManager.room_id) {
      std::vector<Room> rooms;
      bool status = false;
      // bool status =
      //     networkManager.get_rooms(rooms); // FIXME: DON'T FETCH ON MAIN
      //     THREAD
      if (status) {
        if (IsKeyPressed(KEY_SPACE) &&
            rooms[selected_room].players != Constants::PLAYERS_MAX) {
          networkManager.room_id =
              selected_room + 1;  // FIXME: Server respond to request
        } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
          selected_room--;
          selected_room %= number_of_rooms;
        } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
          selected_room++;
          selected_room %= number_of_rooms;
        }
      }
      TraceLog(LOG_DEBUG, "SELECTED ROOM: %d", selected_room);
    } else {
      gameManager.UpdateGame();
      if (gameManager.ReturnToRooms()) networkManager.room_id = 0;
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (!networkManager.room_id) {
      graphicsManager.DrawRoomTitle();
      graphicsManager.DrawRoomSubTitle();
      std::vector<Room> rooms;
      bool status = false;
      // bool status =
      //     networkManager.get_rooms(rooms);
      // FIXME: DON'T FETCH ON MAIN THREAD
      if (status) {
        graphicsManager.DrawRooms(rooms, selected_room);
      }
      graphicsManager.DrawRoomBottomText();
    } else {
      graphicsManager.DrawGame(gameManager);
    }

    EndDrawing();
  }
};
