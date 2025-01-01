#pragma once
#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameManager.hpp"
#include "lobbyManager.hpp"
#include "player.hpp"
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
  void DrawTimer(const LobbyManager &lm);
  void DrawTitle(const LobbyManager &lm);
  void DrawLobbyPlayers(const LobbyManager &lm);
  void DrawReadyMessage();

  // player
  void DrawPlayer(const Player &player);

  // asteroids
  void DrawAsteroid(const Asteroid &asteroid);

  // bullet
  void DrawBullet(const Bullet &bullet);
};
