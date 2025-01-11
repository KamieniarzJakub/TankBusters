#include <atomic>
#include <chrono>
#include <cstdint>
#include <raylib.h>
#include <raymath.h>

#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "roomManager.hpp"

using namespace std::chrono;

struct Game {
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;
  RoomManager roomManager;

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
        roomManager(RoomManager(networkManager)),
        gameManagersPair(std::array<GameManager, 2>()),
        roomsPair(std::array<std::vector<Room>, 2>()),
        playersPair(std::array<std::vector<Player>, 2>()),
        asteroidsPair(std::array<std::vector<Asteroid>, 2>()),
        bulletsPair(std::array<std::vector<Bullet>, 2>()) {}

  void updateDrawFrame(void) {

    if (networkManager.room_id == 0) {
      // Queue network work
      if (steady_clock::now() - roomManager.last_room_fetch >
          roomManager.room_fetch_interval) {
        networkManager.todo.push([&]() {
          bool status = networkManager.get_rooms(
              roomsPair.at(get_networks_idx(rooms_draw_idx)));
          if (status) {
            // Automatically change drawing to the other room
            rooms_draw_idx.exchange(!rooms_draw_idx);
          }
          roomManager.last_room_fetch = steady_clock::now();
          return status;
        });
      }
      roomManager.setSelectedRoom(roomsPair.at(rooms_draw_idx));
    } else {
      gameManagersPair.at(game_manager_draw_idx).UpdateGame();
      if (gameManagersPair.at(game_manager_draw_idx).ReturnToRooms())
        networkManager.room_id = 0;
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (!networkManager.room_id) {
      graphicsManager.DrawRoomTitle();
      graphicsManager.DrawRoomSubTitle();
      graphicsManager.DrawRooms(roomsPair.at(rooms_draw_idx),
                                roomManager.selected_room);
      graphicsManager.DrawRoomBottomText();
    } else {
      graphicsManager.DrawGame(gameManagersPair.at(game_manager_draw_idx));
    }

    EndDrawing();
  }
};
