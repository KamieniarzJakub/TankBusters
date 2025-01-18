#include <raylib.h>
#include <raymath.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "bullet.hpp"
#include "constants.hpp"
#include "gameManager.hpp"
#include "gameStatus.hpp"
#include "graphicsManager.hpp"
#include "networking.hpp"
#include "player.hpp"
#include "room.hpp"

using namespace std::chrono;

struct Game {
  // Does all the drawing
  GraphicsManager graphicsManager;

  // Does all the networking
  ClientNetworkManager networkManager;

  // Display and update game
  std::array<GameManager, 2> gameManagersPair;
  std::atomic_uint8_t game_manager_draw_idx = 0;

  // Display and update rooms on main menu
  std::array<std::map<uint32_t, Room>, 2> roomsMapPair;
  std::atomic_uint8_t rooms_draw_idx = 0;

  // Display and update room while in game lobby
  std::array<Room, 2> joinedRoomPair;
  std::atomic_uint8_t joined_room_draw_idx = 0;

  // Display and update player id
  std::atomic_uint32_t player_id = -1;

  // Last time the room was fetched on main menu
  std::chrono::steady_clock::time_point last_room_fetch =
      std::chrono::steady_clock::time_point{};
  uint8_t selected_room_index = 0;
  std::vector<Room> rooms_vec;

  time_point<steady_clock> game_start_time = std::chrono::steady_clock::now();
  time_point<steady_clock> frame_start_time = std::chrono::steady_clock::now();
  duration<double> frametime = game_start_time - steady_clock::now();

  inline uint8_t get_networks_idx(std::atomic_uint8_t &draw_idx) {
    return !draw_idx.load();
  }

  GameManager &gameManager() {
    return gameManagersPair.at(game_manager_draw_idx);
  }
  std::map<uint32_t, Room> &rooms() { return roomsMapPair.at(rooms_draw_idx); }
  Room &joinedRoom() { return joinedRoomPair.at(joined_room_draw_idx); }

  Game(const char *host, const char *port)
      : graphicsManager(GraphicsManager()),
        networkManager(host, port, gameManagersPair, game_manager_draw_idx,
                       roomsMapPair, rooms_draw_idx, joinedRoomPair,
                       joined_room_draw_idx, player_id),
        gameManagersPair(std::array<GameManager, 2>()),
        roomsMapPair(std::array<std::map<uint32_t, Room>, 2>()),
        joinedRoomPair(std::array<Room, 2>()) {}

  void UpdateLobby() {
    if (ReturnToRooms()) {
      networkManager.todo.push([&]() {
        TraceLog(LOG_DEBUG, "NET: leave room");
        bool status = networkManager.leave_room();
        if (status) {
          std::cout << json(roomsMapPair).dump() << std::endl;

          std::map<uint32_t, Room> rooms;
          networkManager.get_rooms(rooms);

          networkManager.rooms() = rooms;
          networkManager.flip_rooms();
          networkManager.rooms() = rooms;
        }
        return status;
      });
    }
    try {
      if (joinedRoom().players.at(player_id.load()).state !=
              PlayerInfo::READY &&
          IsKeyPressed(KEY_SPACE)) {
        networkManager.todo.push([&]() {
          std::vector<PlayerIdState> player_status;
          TraceLog(LOG_DEBUG, "NET: sending vote ready");
          bool status = networkManager.vote_ready(player_status);
          if (status) {
            networkManager.joinedRoom().players = player_status;
            networkManager.flip_joined_room();
            networkManager.joinedRoom().players = player_status;
          }
          return status;
        });
      }
    } catch (const std::out_of_range &ex) {
    }
  }

  void UpdateMainMenu() {
    if (steady_clock::now() - last_room_fetch >
        Constants::ROOM_FETCH_INTERVAL) {
      last_room_fetch = steady_clock::now();
      networkManager.todo.push([&]() {
        TraceLog(LOG_DEBUG, "NET: Fetch rooms");
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
  }

  void updateDrawFrame(void) {
    auto frame_start_time1 = std::chrono::steady_clock::now();
    frametime = frame_start_time1 - frame_start_time;
    frame_start_time = frame_start_time1;

    if (joinedRoom().room_id == 0) {
      UpdateMainMenu();
    } else if (joinedRoom().status == GameStatus::LOBBY) {
      UpdateLobby();
    } else {
      UpdateGame();
    }

    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (joinedRoom().room_id == 0) {
      graphicsManager.DrawRoomTitle();
      graphicsManager.DrawRoomSubTitle();
      graphicsManager.DrawRooms(rooms_vec, selected_room_index);
      graphicsManager.DrawRoomBottomText();
    } else {
      // graphicsManager.DrawGame(gameManagersPair.at(game_manager_draw_idx));
      if (joinedRoom().status == GameStatus::GAME) {
        graphicsManager.DrawAsteroids(gameManager());
        graphicsManager.DrawPlayers(gameManager());
        graphicsManager.DrawBullets(gameManager());
        graphicsManager.DrawBulletsGUI(gameManager(), player_id.load());
        // graphicsManager.DrawTime(gameManager(), joinedRoom());
      } else if (joinedRoom().status == GameStatus::END_OF_ROUND) {
        graphicsManager.DrawWinnerText(gameManager());
        graphicsManager.DrawNewRoundCountdown(gameManager());
        // graphicsManager.DrawTime(gameManager(), joinedRoom());
      } else {
        graphicsManager.DrawTitle(joinedRoom());
        graphicsManager.DrawLobbyPlayers(joinedRoom());
        graphicsManager.DrawReadyMessage();
        auto t = duration_cast<seconds>(gameManager().game_start_time -
                                        system_clock::now())
                     .count();
        if (t >= 0) {
          graphicsManager.DrawTimer(t);
        } else {
          graphicsManager.DrawExitLobbyMessage();
        }
      }
    }
    EndDrawing();
  }

  void UpdateGame() {
    // gameManager().UpdateStatus();
    if (joinedRoom().status == GameStatus::GAME) {

      // gameManager().ManageCollisions();

      auto &players = gameManager().players;
      auto &player = players.at(player_id.load());
      CheckMovementUpdatePlayer(player, frametime);
      for (auto &p : gameManager().players) { // TODO: better position merging
                                              // and movement interpolation
        CalculateUpdatePlayerMovement(p, frametime);
      }
      networkManager.todo.push([&]() {
        TraceLog(LOG_DEBUG, "NET: sending movement");
        bool status = networkManager.send_movement(
            player.position, player.velocity, player.rotation);
        return status;
      });

      if (Shoot()) {
        // if (players.at(gameManager().player_id).active && Shoot()) {
        // if (gameManager().AddBullet(players.at(gameManager().player_id))) {
        // }
        networkManager.todo.push([&]() {
          TraceLog(LOG_DEBUG, "NET: sending shooting");
          return networkManager.shoot_bullet();
        });
      }

      // TODO: better position merging and movement interpolation
      gameManager().UpdateBullets(frametime);
      gameManager().UpdateAsteroids(frametime);

      // gameManager.AsteroidSpawner(GetTime());
    } else { // Lobby or end of Round
      // if (gameManager().UpdateLobbyStatus(
      //         rooms().at(networkManager.room_id).players)) {
      // NewGame(GetReadyPlayers());
      // gameManager().RestartLobby();
      // }
    }
  }

  void setSelectedRoom() {

    rooms_vec.clear();
    rooms_vec.reserve(rooms().size());
    for (auto r : rooms()) {
      rooms_vec.push_back(r.second);
    }

    try {
      Room &selected_room = rooms_vec.at(selected_room_index);
      if (IsKeyPressed(KEY_SPACE) &&
          get_X_players(selected_room.players, PlayerInfo::NONE) > 0) {
        networkManager.todo.push([&]() {
          TraceLog(LOG_DEBUG, "NET: join room");
          try {
            uint32_t player_id_received;
            bool status = networkManager.join_room(selected_room.room_id,
                                                   player_id_received);
            if (status) {
              uint32_t expected_player_id = -1;
              while (!this->player_id.compare_exchange_strong(
                  expected_player_id, player_id_received)) {
              }
            }
            return status;
          } catch (const std::out_of_range &ex) {
            return false;
          }
        });
      } else if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selected_room_index--;
        selected_room_index %= rooms_vec.size();
      } else if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selected_room_index++;
        selected_room_index %= rooms_vec.size();
      }
    } catch (const std::out_of_range &ex) {
      return;
    }
  }
};
