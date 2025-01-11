#pragma once
#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameManager.hpp"
#include "player.hpp"
#include "room.hpp"
#include <raylib.h>

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
  void DrawTimer(const GameManager &gm);
  void DrawTitle(const GameManager &gm);
  void DrawLobbyPlayers(const GameManager &gm);
  void DrawReadyMessage();
  void DrawExitLobbyMessage();
  void DrawLobby(const GameManager &gm);

  // player
  void DrawPlayer(const Player &player);

  // asteroids
  void DrawAsteroid(const Asteroid &asteroid);

  // bullet
  void DrawBullet(const Bullet &bullet);

  // Game
  void DrawGame(GameManager gameManager);

  // Room
  void DrawRoomTitle();
  void DrawRoomSubTitle();
  void DrawRoomBottomText();
  void DrawRooms(const std::vector<Room> &rooms, int selected);
};
