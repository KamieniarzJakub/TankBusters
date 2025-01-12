#include <raylib.h>
#include <raymath.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "gameManager.hpp"
#include "gameStatus.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"
#include "player.hpp"
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
    return !draw_idx.load();
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
    TraceLog(LOG_INFO, "udf");
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
    } else if (gameManager().status == GameStatus::LOBBY) {
      if (gameManagersPair.at(game_manager_draw_idx).ReturnToRooms()) {
        TraceLog(LOG_INFO, "NET: push leave room");
        networkManager.todo.push([&]() {
          bool status = networkManager.leave_room();
          if (status) {
            std::cout << json(roomsPair).dump() << std::endl;

            std::map<uint32_t, Room> rooms;
            networkManager.get_rooms(rooms);

            networkManager.rooms() = rooms;
            networkManager.flip_rooms();
            networkManager.rooms() = rooms;
          }
          return status;
        });
      }
      if (rooms().at(networkManager.room_id)
                  .players.at(gameManager().player_id)
                  .state != PlayerInfo::READY &&
          IsKeyPressed(KEY_SPACE)) {
        TraceLog(LOG_INFO, "NET: push vote ready");
        networkManager.todo.push([&]() {
          std::vector<PlayerShortInfo> player_status;
          TraceLog(LOG_INFO, "NET: sending vote ready");
          bool status = networkManager.vote_ready(player_status);
          if (status) {
            TraceLog(LOG_INFO, "GAME: player status %s",
                     json(player_status).dump().c_str());
            networkManager.rooms().at(networkManager.room_id).players =
                player_status;
            networkManager.flip_rooms();
            networkManager.rooms().at(networkManager.room_id).players =
                player_status;
            // networkManager.flip_game_manager();
          }
          return status;
        });
      }
    } else {
      UpdateGame();
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
        // TraceLog(LOG_INFO,
        //          json(rooms().at(networkManager.room_id)).dump().c_str());
        graphicsManager.DrawTitle(rooms().at(networkManager.room_id));
        graphicsManager.DrawLobbyPlayers(rooms().at(networkManager.room_id));
        graphicsManager.DrawReadyMessage();
        graphicsManager.DrawTimer(gameManager());
      }
    }
    EndDrawing();
  }

  void UpdateGame() {
    TraceLog(LOG_INFO, "0. Game status: %d", gameManager().status);
    gameManager().UpdateStatus();
    TraceLog(LOG_INFO, "1. Game status: %d", gameManager().status);
    if (gameManager().status == GameStatus::GAME) {
      float frametime = GetFrameTime();

      gameManager().ManageCollisions();
      TraceLog(LOG_INFO, "2. Game status: %d", gameManager().status);

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
        // FIXME: update game objects
        return status;
      });

      if (players.at(gameManager().player_id).active && Shoot()) {
        if (gameManager().AddBullet(players.at(gameManager().player_id))) {
          TraceLog(LOG_INFO, "NET: push shoot");
          networkManager.todo.push([&]() {
            return networkManager.shoot_bullet();
            // FIXME: update game objects
          });
        }
      }

      gameManager().UpdateBullets(frametime);
      gameManager().UpdateAsteroids(frametime);

      // gameManager.AsteroidSpawner(GetTime());
    } else { // Lobby or end of Round
      TraceLog(LOG_INFO, "3. Game status: %d", gameManager().status);
      if (gameManager().UpdateLobbyStatus(
              rooms().at(networkManager.room_id).players)) {
        TraceLog(LOG_INFO, "4. Game status: %d", gameManager().status);
        // NewGame(GetReadyPlayers());
        gameManager().RestartLobby();
        TraceLog(LOG_INFO, "5. Game status: %d", gameManager().status);
      }
      TraceLog(LOG_INFO, "6. Game status: %d", gameManager().status);
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
              networkManager.flip_game_manager();
              networkManager.gameManager().player_id = player_id;

              Room room;
              bool status = networkManager.fetch_room_state(room);

              if (status) {
                // TraceLog(LOG_INFO, "GAME: room %s",
                // json(room).dump().c_str());

                networkManager.rooms().at(networkManager.room_id) = room;
                networkManager.flip_rooms();
                networkManager.rooms().at(networkManager.room_id) = room;

                // std::cout << json(roomsPair).dump() << std::endl;
                // TraceLog(LOG_INFO, "GAME: room status: %s",
                //          json(networkManager.rooms().at(networkManager.room_id))
                //              .dump()
                //              .c_str());
              }
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
