#pragma once
#include <raylib.h>

#include <cstdint>

#include "asteroid.hpp"
#include "bullet.hpp"
#include "player.hpp"
#include "room.hpp"

struct GraphicsManager {
  Font font;

  GraphicsManager();
  ~GraphicsManager();

  // Draw main menu
  void DrawRoomTitle();
  void DrawRoomSubTitle();
  void DrawRoomBottomText();
  void DrawRooms(const std::vector<Room> &rooms,
                 uint32_t selected_room_index_not_id);

  // Draw lobby
  void DrawTimer(const char *message, time_point<system_clock> when);
  void DrawTitle(const Room &r);
  void DrawLobbyPlayers(const Room &r);
  void DrawReadyMessage();
  void DrawExitLobbyMessage();
  void DrawWinnerText(const uint32_t winner_player_id);

  // Draw Game
  void DrawAsteroids(const std::vector<Asteroid> &asteroids);
  void DrawPlayers(const std::vector<Player> &players);
  void DrawBullets(const std::vector<Bullet> &bullets);
  void DrawBulletsGUI(const std::vector<Bullet> &bullets,
                      const uint32_t player_id);
  void DrawPlayer(const Player &player);
  void DrawAsteroid(const Asteroid &asteroid);
  void DrawBullet(const Bullet &bullet);
};
