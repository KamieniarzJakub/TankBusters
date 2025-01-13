#pragma once

#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameStatus.hpp"
#include "player.hpp"
#include "room.hpp"
#include <raylib.h>
#include <raymath.h>
#include <sys/types.h>
#include <vector>

struct GameManager {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  std::vector<Bullet> bullets;

  uint32_t player_id;
  uint32_t room_id;
  GameStatus status;
  int _alive_players;
  float _spawnerTime;
  float startRoundTime;
  float endRoundTime;

  uint32_t game_start_time;

  GameManager();
  GameManager(uint32_t room_id, std::vector<PlayerShortInfo> playerInfos);
  ~GameManager();

  // void NewGame(int players_in_game = 0);
  void NewGame(std::vector<PlayerShortInfo> playerInfos);

  void UpdateStatus();
  void UpdatePlayers(float frametime);
  void UpdateBullets(float frametime);
  void UpdateAsteroids(float frametime);
  void UpdateGameServer();

  void AsteroidSpawner(double time);

  void ManageCollisions();

  void AddAsteroid();
  void SplitAsteroid(Vector2 position, Vector2 velocity, int size);
  bool AddBullet(const Player &player);

  void RestartLobby();

  void UpdatePlayersLobby();

  bool ReturnToRooms();
  size_t
  GetReadyPlayers(const std::vector<PlayerShortInfo> &player_infos) const;
  // bool UpdateLobbyStatus(const std::vector<PlayerShortInfo> &player_infos);
};

void to_json(json &j, const GameManager &gm);
void from_json(const json &j, GameManager &gm);
