#pragma once

#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameStatus.hpp"
#include "player.hpp"
#include <raylib.h>
#include <raymath.h>
#include <vector>

struct GameManager {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  std::vector<Bullet> bullets;

  uint32_t room_id;
  GameStatus status;
  int _alive_players;
  float _spawnerTime;
  float startRoundTime;
  float endRoundTime;
  float new_round_timer;

  GameManager();
  GameManager(uint32_t room_id, uint32_t player_number);
  ~GameManager();

  void NewGame(int players_in_game = 0);

  void UpdateStatus();
  void UpdatePlayers(float frametime);
  void UpdateBullets(float frametime);
  void UpdateAsteroids(float frametime);
  void UpdateGame();

  void AsteroidSpawner(double time);

  void ManageCollisions();

  void AddAsteroid();
  void SplitAsteroid(Vector2 position, Vector2 velocity, int size);
  bool AddBullet(const Player &player, int player_number);

  void RestartLobby();

  void UpdatePlayersLobby();

  bool UpdateLobbyStatus();
  bool ReturnToRooms();
  size_t GetReadyPlayers();
  size_t GetConnectedPlayers(PlayerConnection pc);
};

void to_json(json &j, const GameManager &gm);
void from_json(const json &j, GameManager &gm);
