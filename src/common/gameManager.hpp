#pragma once

#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameStatus.hpp"
#include "player.hpp"
#include "room.hpp"
#include <chrono>
#include <raylib.h>
#include <raymath.h>
#include <sys/types.h>
#include <vector>

using namespace std::chrono;

struct GameManager {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  std::vector<Bullet> bullets;

  uint32_t player_id;
  uint32_t room_id;
  GameStatus status;
  int _alive_players;
  time_point<steady_clock> _spawnerTime;
  time_point<steady_clock> startRoundTime;
  time_point<steady_clock> endRoundTime;
  void UpdatePlayers(duration<double> frametime);

  uint32_t game_start_time;

  GameManager();
  GameManager(uint32_t room_id, std::vector<PlayerShortInfo> playerInfos);
  ~GameManager();

  // void NewGame(int players_in_game = 0);
  void NewGame(std::vector<PlayerShortInfo> playerInfos);

  void UpdateStatus();
  void UpdateGameServer();

  void UpdateAsteroids(duration<double> frametime);
  bool AsteroidSpawner();

  void ManageCollisions();

  void AddAsteroid();
  void SplitAsteroid(Vector2 position, Vector2 velocity, int size);
  bool AddBullet(const Player &player);
  void UpdateBullets(duration<double> frametime);

  void RestartLobby();

  void UpdatePlayersLobby();

  bool ReturnToRooms();
  size_t
  GetReadyPlayers(const std::vector<PlayerShortInfo> &player_infos) const;
  // bool UpdateLobbyStatus(const std::vector<PlayerShortInfo> &player_infos);
};

void to_json(json &j, const GameManager &gm);
void from_json(const json &j, GameManager &gm);
