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

  // game manager
  void DrawAsteroids(const GameManager &gm);
  void DrawPlayers(const GameManager &gm);
  void DrawBullets(const GameManager &gm);
  void DrawTime(const GameManager &gm, double time);
  void DrawWinnerText(const GameManager &gm);
  void DrawNewRoundCountdown(const GameManager &gm);

  // lobby manager
  void DrawTimer(uint32_t t);
  void DrawTitle(const Room &r);
  void DrawLobbyPlayers(const Room &r);
  void DrawReadyMessage();
  void DrawExitLobbyMessage();

  // player
  void DrawPlayer(const Player &player);

  // asteroids
  void DrawAsteroid(const Asteroid &asteroid);

  // bullet
  void DrawBullet(const Bullet &bullet);

  // Game
  void DrawGame(GameManager gameManager, Room room);

  // Room
  void DrawRoomTitle();
  void DrawRoomSubTitle();
  void DrawRoomBottomText();
  void DrawRooms(const std::map<uint32_t, Room> &rooms,
                 uint32_t selected_room_id);
};
