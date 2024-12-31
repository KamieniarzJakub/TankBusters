#pragma once

#include "asteroid.hpp"
#include "bullet.hpp"
#include "player.hpp"
#include <raylib.h>
#include <raymath.h>
#include <vector>

enum Status { LOBBY = 0, GAME = 1, END_OF_ROUND = 2 };

struct GameManager {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  std::vector<Bullet> bullets;

  int status;
  int _alive_players;
  float _spawnerTime;
  float startRoundTime;
  float endRoundTime;
  Texture player_texture;
  Font font;
  Font win_font;
  Font player_font;

  GameManager();
  ~GameManager();

  void NewGame(int players_in_game = 0);

  void UpdateGameStatus();
  void UpdatePlayers(float frametime);
  void UpdateBullets(float frametime);
  void UpdateAsteroids(float frametime);

  void AsteroidSpawner(double time);

  void DrawAsteroids();
  void DrawPlayers();
  void DrawBullets();
  void DrawTime(double time);
  void DrawWinnerText();
  void DrawNewRoundCountdown();

  void ManageCollisions();

  void AddAsteroid();
  void SplitAsteroid(Vector2 position, Vector2 velocity, int size);
  void AddBullet(Player player, int player_number);
};
