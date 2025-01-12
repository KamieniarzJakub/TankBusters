#include <raylib.h>
#include <raymath.h>

#include <atomic>
#include <chrono>
#include <cstdint>

#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"

using namespace std::chrono;

struct Game {
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;

  std::array<GameManager, 2> gameManagersPair;
  std::atomic_uint8_t game_manager_draw_idx = 0;

  std::array<std::vector<Room>, 2> roomsPair;
  std::atomic_uint8_t rooms_draw_idx = 0;

  const std::chrono::milliseconds room_fetch_interval{5000};
  std::chrono::steady_clock::time_point last_room_fetch =
      std::chrono::steady_clock::time_point{};
  uint8_t selected_room;

  inline uint8_t get_networks_idx(std::atomic_uint8_t &draw_idx) {
    return draw_idx.fetch_xor(true);
  }

  GameManager &gameManager() {
    return gameManagersPair.at(game_manager_draw_idx);
  }
  std::vector<Room> &rooms() { return roomsPair.at(rooms_draw_idx); }

  Game(const char *host, const char *port)
      : graphicsManager(GraphicsManager()),
        networkManager(host, port, gameManagersPair, game_manager_draw_idx,
                       roomsPair, rooms_draw_idx),
        gameManagersPair(std::array<GameManager, 2>()),
        roomsPair(std::array<std::vector<Room>, 2>()) {}

  void updateDrawFrame(void) {
    if (networkManager.room_id == 0) {
      // Queue network work
      if (steady_clock::now() - last_room_fetch >
          room_fetch_interval) {  // TODO: use timerfd
        last_room_fetch = steady_clock::now();
        TraceLog(LOG_INFO, "NET: push fetch rooms");
        networkManager.todo.push([&]() {
          bool status = networkManager.get_rooms(networkManager.rooms());
          if (status) {
            // Automatically change drawing to the other room
            networkManager.flip_rooms();
          }
          return status;
        });
      }
      setSelectedRoom(roomsPair.at(rooms_draw_idx));
    } else {
      UpdateGame(gameManager());
      if (gameManagersPair.at(game_manager_draw_idx).ReturnToRooms()) {
        TraceLog(LOG_INFO, "NET: push leave room");
        networkManager.todo.push([&]() { return networkManager.leave_room(); });
      }
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (!networkManager.room_id) {
      graphicsManager.DrawRoomTitle();
      graphicsManager.DrawRoomSubTitle();
      graphicsManager.DrawRooms(roomsPair.at(rooms_draw_idx), selected_room);
      graphicsManager.DrawRoomBottomText();
    } else {
      graphicsManager.DrawGame(gameManagersPair.at(game_manager_draw_idx));
    }

    EndDrawing();
  }

  void UpdateGame(GameManager gameManager) {
    gameManager.UpdateStatus();
    // TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
    if (gameManager.status == GameStatus::GAME) {
      float frametime = GetFrameTime();

      gameManager.ManageCollisions();

      auto &players = gameManager.players;
      auto &player = players.at(gameManager.player_id);
      UpdatePlayer(players.at(gameManager.player_id), frametime);
      TraceLog(LOG_INFO, "NET: push send movement");
      networkManager.todo.push([&]() {
        return networkManager.send_movement(player.position, player.velocity,
                                            player.rotation);
      });

      if (players.at(gameManager.player_id).active && Shoot()) {
        if (gameManager.AddBullet(players.at(gameManager.player_id))) {
          TraceLog(LOG_INFO, "NET: push shoot");
          networkManager.todo.push(
              [&]() { return networkManager.shoot_bullet(); });
        }
      }

      gameManager.UpdateBullets(frametime);
      gameManager.UpdateAsteroids(frametime);

      // gameManager.AsteroidSpawner(GetTime());
    } else {  // Lobby or end of Round
      if (gameManager.UpdateLobbyStatus()) {
        // NewGame(GetReadyPlayers());
        gameManager.RestartLobby();
      }
    }
  }

  void setSelectedRoom(const std::vector<Room> &rooms) {
    if (IsKeyPressed(KEY_SPACE) &&
        rooms.at(selected_room).players != Constants::PLAYERS_MAX) {
      TraceLog(LOG_INFO, "NET: push join room");
      networkManager.todo.push([&]() {
        bool status =
            networkManager.join_room(rooms.at(selected_room).room_id,
                                     networkManager.gameManager().player_id);
        if (status) {
          networkManager.flip_game_manager();
        }
        return status;
      });
    } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
      selected_room--;
      selected_room %= rooms.size();
    } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
      selected_room++;
      selected_room %= rooms.size();
    }
    // TraceLog(LOG_DEBUG, "SELECTED ROOM: %d", selected_room);
  }
};
