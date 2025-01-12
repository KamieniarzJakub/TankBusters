#include <raylib.h>
#include <raymath.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "gameManager.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"
#include "room.hpp"

using namespace std::chrono;

struct Game {
  GraphicsManager graphicsManager;
  ClientNetworkManager networkManager;

  std::array<GameManager, 2> gameManagersPair;
  std::atomic_uint8_t game_manager_draw_idx = 0;

  std::array<std::map<uint32_t, Room>, 2> roomsPair;
  std::atomic_uint8_t rooms_draw_idx = 0;

  const std::chrono::milliseconds room_fetch_interval{5000};
  std::chrono::steady_clock::time_point last_room_fetch =
      std::chrono::steady_clock::time_point{};
  uint8_t selected_room_index = 0;
  Room selected_room{};

  inline uint8_t get_networks_idx(std::atomic_uint8_t &draw_idx) {
    return draw_idx.fetch_xor(true);
  }

  GameManager &gameManager() {
    return gameManagersPair.at(game_manager_draw_idx);
  }
  std::map<uint32_t, Room> &rooms() { return roomsPair.at(rooms_draw_idx); }

  Game(const char *host, const char *port)
      : graphicsManager(GraphicsManager()),
        networkManager(host, port, gameManagersPair, game_manager_draw_idx,
                       roomsPair, rooms_draw_idx),
        gameManagersPair(std::array<GameManager, 2>()),
        roomsPair(std::array<std::map<uint32_t, Room>, 2>()) {}

  void updateDrawFrame(void) {
    if (networkManager.room_id == 0) {
      // Queue network work
      if (steady_clock::now() - last_room_fetch >
          room_fetch_interval) { // TODO: use timerfd
        last_room_fetch = steady_clock::now();
        TraceLog(LOG_INFO, "NET: push fetch rooms");
        networkManager.todo.push([&]() {
          std::map<uint32_t, Room> update_rooms;
          bool status = networkManager.get_rooms(update_rooms);
          if (status) {
            networkManager.rooms() = update_rooms;
            networkManager.flip_rooms();
            networkManager.rooms() = update_rooms;
          }
          return status;
        });
      }
      setSelectedRoom();
    } else {
      UpdateGame();
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
      graphicsManager.DrawRooms(rooms(), selected_room_index);
      graphicsManager.DrawRoomBottomText();
    } else {
      // graphicsManager.DrawGame(gameManagersPair.at(game_manager_draw_idx));
      if (gameManager().status == GameStatus::GAME) {
        graphicsManager.DrawAsteroids(gameManager());
        graphicsManager.DrawPlayers(gameManager());
        graphicsManager.DrawBullets(gameManager());
        graphicsManager.DrawTime(gameManager(), GetTime());
      } else if (gameManager().status == GameStatus::END_OF_ROUND) {
        graphicsManager.DrawWinnerText(gameManager());
        graphicsManager.DrawNewRoundCountdown(gameManager());
        graphicsManager.DrawTime(gameManager(), GetTime());
      } else {
        if (IsKeyPressed(KEY_SPACE)) {
          TraceLog(LOG_INFO, "NET: push vote ready");
          networkManager.todo.push([&]() {
            bool status = networkManager.vote_ready(
                networkManager.rooms().at(networkManager.room_id).players);
            if (status) {
              networkManager.flip_game_manager();
            }
            return status;
          });
        }
        // TraceLog(LOG_INFO,
        //          json(rooms().at(networkManager.room_id)).dump().c_str());
        graphicsManager.DrawTitle(rooms().at(networkManager.room_id));
        graphicsManager.DrawLobbyPlayers(gameManager());
        graphicsManager.DrawReadyMessage();
        graphicsManager.DrawTimer(gameManager());
      }
    }
    EndDrawing();
  }

  void UpdateGame() {
    gameManager().UpdateStatus();
    // TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
    if (gameManager().status == GameStatus::GAME) {
      float frametime = GetFrameTime();

      gameManager().ManageCollisions();

      auto &players = gameManager().players;
      auto &player = players.at(gameManager().player_id);
      UpdatePlayer(players.at(gameManager().player_id), frametime);
      TraceLog(LOG_INFO, "NET: push send movement");
      networkManager.todo.push([&]() {
        // networkmanager.gamemanager().player_id = player_id;
        //       networkmanager.rooms()
        //           .at(networkmanager.room_id)
        //           .players.at(player_id)
        //           .state = playerinfo::not_ready;
        //       networkmanager.flip_rooms();
        //       // networkmanager.gamemanager().player_id =
        //       networkmanager.flip_game_manager();
        //       networkmanager.gamemanager().player_id = player_id;
        //       networkmanager.rooms()
        //           .at(networkmanager.room_id)
        //           .players.at(player_id)
        //           .state = playerinfo::not_ready;

        bool status = networkManager.send_movement(
            player.position, player.velocity, player.rotation);
        return status;
      });

      if (players.at(gameManager().player_id).active && Shoot()) {
        if (gameManager().AddBullet(players.at(gameManager().player_id))) {
          TraceLog(LOG_INFO, "NET: push shoot");
          networkManager.todo.push(
              [&]() { return networkManager.shoot_bullet(); });
        }
      }

      gameManager().UpdateBullets(frametime);
      gameManager().UpdateAsteroids(frametime);

      // gameManager.AsteroidSpawner(GetTime());
    } else { // Lobby or end of Round
      if (gameManager().UpdateLobbyStatus(
              rooms().at(networkManager.room_id).players)) {
        // NewGame(GetReadyPlayers());
        gameManager().RestartLobby();
      }
    }
  }

  void setSelectedRoom() {
    uint8_t iter = 0;
    for (auto r : rooms()) {
      if (selected_room_index == iter++) {
        selected_room = r.second;
        break;
      }
    }

    try {
      if (IsKeyPressed(KEY_SPACE) &&
          get_X_players(selected_room.players, PlayerInfo::NONE) > 0) {
        TraceLog(LOG_INFO, "NET: push join room");
        networkManager.todo.push([&]() {
          try {
            uint32_t player_id;
            bool status =
                networkManager.join_room(selected_room.room_id, player_id);
            if (status) {
              networkManager.gameManager().player_id = player_id;
              networkManager.rooms()
                  .at(networkManager.room_id)
                  .players.at(player_id)
                  .state = PlayerInfo::NOT_READY;
              networkManager.flip_rooms();
              // networkManager.gameManager().player_id =
              networkManager.flip_game_manager();
              networkManager.gameManager().player_id = player_id;
              networkManager.rooms()
                  .at(networkManager.room_id)
                  .players.at(player_id)
                  .state = PlayerInfo::NOT_READY;
              TraceLog(LOG_INFO, "GAME: room status: %s",
                       json(rooms().at(networkManager.room_id)).dump().c_str());
            }
            return status;
          } catch (const std::out_of_range &ex) {
            return false;
          }
        });
      } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_room_index--;
        selected_room_index %= rooms().size();
      } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_room_index++;
        selected_room_index %= rooms().size();
      }
      // TraceLog(LOG_DEBUG, "SELECTED ROOM: %d", selected_room);
    } catch (const std::out_of_range &ex) {
      return;
    }
  }
};
