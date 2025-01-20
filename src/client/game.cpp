#include <atomic>
#include <chrono>
#include <cstdint>
#include <raylib.h>
#include <raymath.h>

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

  time_point<steady_clock> last_position_time_sent = steady_clock::now();

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
          networkManager.get_rooms();
        }
      });
    }
    try {
      auto my_player_state = joinedRoom().players.at(player_id.load()).state;
      if (my_player_state != PlayerInfo::READY && IsKeyPressed(KEY_SPACE)) {
        networkManager.todo.push([&]() {
          TraceLog(LOG_DEBUG, "NET: sending vote ready");
          networkManager.send_vote_ready();
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
        networkManager.get_rooms();
      });
    }
    setSelectedRoom();
  }

  void updateDrawFrame() {
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

    DrawFrame();
  }

  void DrawFrame() {
    BeginDrawing();

    ClearBackground(Constants::BACKGROUND_COLOR);

    if (joinedRoom().room_id == 0) {
      graphicsManager.DrawRoomTitle();
      graphicsManager.DrawRoomSubTitle();
      graphicsManager.DrawRooms(rooms_vec, selected_room_index);
      graphicsManager.DrawRoomBottomText();
    } else {
      switch (joinedRoom().status) {
      case GameStatus::LOBBY: {
        graphicsManager.DrawTitle(joinedRoom());
        graphicsManager.DrawLobbyPlayers(joinedRoom());
        if (joinedRoom().players.at(player_id.load()).state ==
            PlayerInfo::NOT_READY) {
          graphicsManager.DrawReadyMessage();
          graphicsManager.DrawExitLobbyMessage();
        }
        graphicsManager.DrawWinnerText(gameManager().winner_player_id);
        if (gameManager().game_start_time > system_clock::now()) {
          graphicsManager.DrawTimer("New game in ",
                                    gameManager().game_start_time + 1s);
        }

      } break;
      case GameStatus::GAME:
        graphicsManager.DrawAsteroids(gameManager().asteroids);
        graphicsManager.DrawPlayers(gameManager().players);
        graphicsManager.DrawBullets(gameManager().bullets);
        graphicsManager.DrawBulletsGUI(gameManager().bullets, player_id.load());
        break;
      case GameStatus::NO_STATUS:
        break;
      }
    }
    EndDrawing();
  }

  void UpdateGame() {

    auto &players = gameManager().players;
    auto &player = players.at(player_id.load());
    CheckMovementUpdatePlayer(player, frametime);
    for (auto &p : gameManager().players) {
      CalculateUpdatePlayerMovement(p, frametime);
    }

    if (steady_clock::now() - last_position_time_sent > 0.1s) {
      last_position_time_sent = steady_clock::now();
      networkManager.todo.push([&]() {
        networkManager.send_movement(player.position, player.velocity,
                                     player.rotation);
      });
    }

    if (Shoot() && gameManager().players.at(player_id.load()).active) {
      networkManager.todo.push([&]() { networkManager.send_shoot_bullet(); });
    }

    gameManager().UpdateBullets(frametime);
    gameManager().UpdateAsteroids(frametime);
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
          if (!networkManager.get_rooms()) {
            return;
          }
          networkManager.join_room(selected_room.room_id);
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
