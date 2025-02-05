#pragma once

#include "asteroid.hpp"
#include "bullet.hpp"
#include "player.hpp"
#include "room.hpp"
#include <chrono>
#include <cstdint>
#include <raylib.h>
#include <sys/types.h>
#include <vector>

using namespace std::chrono;

struct GameManager {
  std::vector<Asteroid> asteroids;
  std::vector<Player> players;
  std::vector<Bullet> bullets;

  uint32_t winner_player_id = UINT32_MAX;
  uint32_t room_id;

  time_point<steady_clock> asteroid_spawner_time;
  time_point<system_clock> game_start_time;

  void UpdatePlayers(duration<double> frametime);
  GameManager();
  GameManager(uint32_t room_id, std::vector<PlayerIdState> playerInfos);
  ~GameManager();

  // void NewGame(int players_in_game = 0);
  void NewGame(std::vector<PlayerIdState> playerInfos);

  void UpdateStatus();
  void UpdateGameServer();

  void UpdateAsteroids(duration<double> frametime);
  void AsteroidSpawner(std::vector<uint32_t> &spawned_asteroids);

  void ManageCollisions(std::vector<uint32_t> &destroyed_asteroids,
                        std::vector<uint32_t> &spawned_asteroids,
                        std::vector<uint32_t> &destroyed_players,
                        std::vector<uint32_t> &destroyed_bullets);
  uint32_t AddAsteroid();
  void SplitAsteroid(Vector2 position, Vector2 velocity, int size,
                     std::vector<uint32_t> &changed);
  bool AddBullet(const Player &player);
  void UpdateBullets(duration<double> frametime);

  size_t GetReadyPlayers(const std::vector<PlayerIdState> &player_infos) const;
};

bool ReturnToRooms();
void to_json(json &j, const GameManager &gm);
void from_json(const json &j, GameManager &gm);
