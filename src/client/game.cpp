#include <atomic>
#include <cstdint>
#include <raylib.h>
#include <raymath.h>

#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"

struct Game {
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;

  std::array<GameManager, 2> gameManagersPair;
  std::atomic_uint8_t game_manager_draw_idx = 0;

  std::array<std::vector<Room>, 2> roomsPair;
  std::atomic_uint8_t rooms_draw_idx = 0;

  std::array<std::vector<Player>, 2> playersPair;
  std::atomic_uint8_t players_draw_idx = 0;

  std::array<std::vector<Asteroid>, 2> asteroidsPair;
  std::atomic_uint8_t asteroids_draw_idx = 0;

  std::array<std::vector<Bullet>, 2> bulletsPair;
  std::atomic_uint8_t bullets_draw_idx = 0;

  inline uint8_t get_networks_idx(std::atomic_uint8_t &draw_idx) {
    return draw_idx.fetch_xor(true);
  }

  Game(const char *host, const char *port)
      : graphicsManager(GraphicsManager()), networkManager(host, port),
        gameManagersPair(std::array<GameManager, 2>()),
        roomsPair(std::array<std::vector<Room>, 2>()),
        playersPair(std::array<std::vector<Player>, 2>()),
        asteroidsPair(std::array<std::vector<Asteroid>, 2>()),
        bulletsPair(std::array<std::vector<Bullet>, 2>()) {
    // std::vector<Room> rooms;
    // bool status = networkManager.get_rooms(rooms);
    // if (status) {
    //   TraceLog(LOG_INFO, ("ROOMS: " + json(rooms).dump()).c_str());
    // } else {
    //   TraceLog(LOG_ERROR, "NET: connection problem");
    // }
  }

  void updateDrawFrame(void) {
    // Queue network work
    networkManager.todo.push([&]() {
      bool status = networkManager.get_rooms(
          roomsPair.at(get_networks_idx(rooms_draw_idx)));
      if (status) {
        // Automatically change drawing to the other room
        rooms_draw_idx.exchange(!rooms_draw_idx);
      }
      return status;
    });

    int selected_room;
    // MAYBE move somewhere else
    if (!networkManager.room_id) {
      int number_of_rooms;
      std::vector<Room> rooms;
      bool status = false;
      // bool status =
      //     networkManager.get_rooms(rooms); // FIXME: DON'T FETCH ON MAIN
      //     THREAD
      if (status) {
        if (IsKeyPressed(KEY_SPACE) &&
            rooms[selected_room].players != Constants::PLAYERS_MAX) {
          networkManager.room_id =
              selected_room + 1; // FIXME: Server respond to request
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
      gameManagersPair[game_manager_draw_idx].UpdateGame();
      if (gameManagersPair[game_manager_draw_idx].ReturnToRooms())
        networkManager.room_id = 0;
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
      graphicsManager.DrawGame(gameManagersPair[game_manager_draw_idx]);
    }

    EndDrawing();
  }
};
