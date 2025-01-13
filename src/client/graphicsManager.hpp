#pragma once
#include <raylib.h>

#include <cstdint>

#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameManager.hpp"
#include "player.hpp"
#include "room.hpp"

struct GraphicsManager {
  Texture player_texture;
  Font font;
  Font win_font;
  Font player_font;

  GraphicsManager();
  ~GraphicsManager();

  // Draw main menu
  void DrawRoomTitle();
  void DrawRoomSubTitle();
  void DrawRoomBottomText();
  void DrawRooms(const std::vector<Room> &rooms,
                 uint32_t selected_room_index_not_id);

  // Draw lobby
  void DrawTimer(uint32_t t);
  void DrawTitle(const Room &r);
  void DrawLobbyPlayers(const Room &r);
  void DrawReadyMessage();
  void DrawExitLobbyMessage();
  void DrawTime(const GameManager &gm, const Room &r);

  // Draw Game
  void DrawGame(const GameManager &gameManager, const Room &room);
  void DrawAsteroids(const GameManager &gm);
  void DrawPlayers(const GameManager &gm);
  void DrawBullets(const GameManager &gm);
  void DrawPlayer(const Player &player);
  void DrawAsteroid(const Asteroid &asteroid);
  void DrawBullet(const Bullet &bullet);

  // Draw new round after game
  void DrawWinnerText(const GameManager &gm);
  void DrawNewRoundCountdown(const GameManager &gm);
};
