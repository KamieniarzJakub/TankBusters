#include "gameManager.hpp"
#include "constants.hpp"
#include "player.hpp"
#include <chrono>

using namespace std::chrono;

GameManager::GameManager() {
  NewGame(std::vector<PlayerIdState>(Constants::PLAYERS_MAX));
}

GameManager::~GameManager() {}

void GameManager::NewGame(std::vector<PlayerIdState> playerInfos) {
  asteroids = std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid());
  players.clear();
  players.reserve(Constants::PLAYERS_MAX);
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    players.push_back(AddPlayer(i));
    players.at(i).active = playerInfos.at(i).state == PlayerInfo::READY;
    players.at(i).player_id = i;
  }
  bullets = std::vector<Bullet>(
      Constants::BULLETS_PER_PLAYER * Constants::PLAYERS_MAX, Bullet());
  _spawnerTime = time_point<steady_clock>(0s);
  // _alive_players = GetReadyPlayers(playerInfos);
  startRoundTime = steady_clock::now();
  endRoundTime = time_point<steady_clock>(0s);
}

void GameManager::UpdateBullets(duration<double> frametime) {
  for (int i = 0; i < Constants::PLAYERS_MAX * Constants::BULLETS_PER_PLAYER;
       i++) {
    UpdateBullet(&bullets[i], frametime);
  }
}

void GameManager::UpdateAsteroids(duration<double> frametime) {
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    UpdateAsteroid(&asteroids[i], frametime);
  }
}

void GameManager::AsteroidSpawner(std::vector<uint32_t> &spawned_asteroids) {
  auto now = std::chrono::steady_clock::now();
  if (now > _spawnerTime + Constants::ASTEROID_SPAWN_DELAY) {
    _spawnerTime = now;
    auto r = AddAsteroid();
    if (r != UINT32_MAX) {
      spawned_asteroids.push_back(r);
    }
  }
}

void GameManager::ManageCollisions(std::vector<uint32_t> &destroyed_asteroids,
                                   std::vector<uint32_t> &spawned_asteroids,
                                   std::vector<uint32_t> &destroyed_players,
                                   std::vector<uint32_t> &destroyed_bullets) {
  // void GameManager::ManageCollisions() {
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    if (!asteroids[i].active)
      continue;

    for (int j = 0; j < Constants::PLAYERS_MAX; j++) {
      for (int k = j * Constants::BULLETS_PER_PLAYER;
           k < (j + 1) * Constants::BULLETS_PER_PLAYER; k++) {
        if (!bullets[k].active)
          continue;

        if (CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE,
                                  asteroids[i].position, asteroids[i].size)) {
          destroyed_bullets.push_back(k);
          destroyed_asteroids.push_back(i);
          bullets[k].active = false;
          asteroids[i].active = false;
          SplitAsteroid(asteroids[i].position, asteroids[i].velocity,
                        float(asteroids[i].size) /
                            Constants::ASTEROID_SPLIT_LOSS,
                        spawned_asteroids);
          break;
        }

        for (int l = 0; l < Constants::PLAYERS_MAX; l++) {
          if (j == l || !players[l].active)
            continue;

          if (CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE,
                                    players[l].position,
                                    Constants::PLAYER_SIZE / 3.0f)) {
            destroyed_players.push_back(l);
            destroyed_bullets.push_back(k);
            players[l].active = false;
            bullets[k].active = false;
            break;
          }
        }
      }

      if (!players[j].active)
        continue;

      if (CheckCollisionCircles(players[j].position,
                                Constants::PLAYER_SIZE / 3.0f,
                                asteroids[i].position, asteroids[i].size)) {
        destroyed_players.push_back(j);
        destroyed_asteroids.push_back(i);
        players[j].active = false;
        asteroids[i].active = false;
        SplitAsteroid(asteroids[i].position, asteroids[i].velocity,
                      float(asteroids[i].size) / Constants::ASTEROID_SPLIT_LOSS,
                      spawned_asteroids);
      }
    }
  }
  // TraceLog(LOG_DEBUG, "Alive players: %d", _alive_players);
}

uint32_t GameManager::AddAsteroid() {
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    if (asteroids[i].active)
      continue;
    asteroids[i] = CreateAsteroid();
    return i;
  }
  TraceLog(LOG_ERROR, "Failed to create an asteroid - no empty slots left");
  return UINT32_MAX;
}

void GameManager::SplitAsteroid(Vector2 position, Vector2 velocity, int size,
                                std::vector<uint32_t> &changed) {
  if (size < Constants::ASTEROID_SIZE_MIN)
    return;

  int toSpawn = 2;
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    if (!toSpawn)
      return;
    if (asteroids[i].active)
      continue;

    Vector2 new_velocity =
        Vector2Rotate(velocity, toSpawn % 2 ? GetRandomValue(60, 120)
                                            : GetRandomValue(-120, -60));
    asteroids[i] = CreateAsteroid(position, new_velocity, size);
    changed.push_back(i);
    toSpawn--;
  }

  TraceLog(LOG_ERROR, "Failed to split an asteroid - no empty slots left");
}

bool GameManager::AddBullet(const Player &player) {
  if (!player.active)
    return false;

  for (size_t i = player.player_id * Constants::BULLETS_PER_PLAYER;
       i < (player.player_id + 1) * Constants::BULLETS_PER_PLAYER; i++) {
    if (bullets.at(i).active)
      continue;

    Vector2 offset = Vector2Rotate(Vector2{Constants::PLAYER_SIZE / 2.0f, 0.0f},
                                   player.rotation * DEG2RAD);
    bullets[i] =
        CreateBullet(Vector2Add(player.position, offset), player.rotation);
    return true;
  }

  TraceLog(LOG_INFO, "Failed to shoot a bullet - player[%d]: no bullets left",
           player.player_id);
  return false;
}

void GameManager::UpdatePlayersLobby() {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    if (IsKeyPressed(KEY_SPACE)) {
      players[i].active = true;
    }
  }
  // TraceLog(LOG_DEBUG, "PLAYERS READY FOR NEW ROUND: %d", ready_players);
}

size_t GameManager::GetReadyPlayers(
    const std::vector<PlayerIdState> &player_infos) const {
  size_t i = 0;
  for (const auto &p : player_infos) {
    if (p.state == PlayerInfo::READY)
      i++;
  }
  return i;
}

// bool GameManager::UpdateLobbyStatus(
//     const std::vector<PlayerShortInfo> &player_infos) {
//   size_t ready_players = GetReadyPlayers(player_infos);
//   if (ready_players > 2) {
//     new_round_timer = new_round_timer > 0 ? new_round_timer : GetTime();
//     if (new_round_timer > 0 &&
//         int(GetTime() - new_round_timer) >= Constants::LOBBY_READY_TIME)
//       return true;
//   } else
//     new_round_timer = -1;
//   return false;
// }

bool ReturnToRooms() {
  return ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)));
}

void GameManager::RestartLobby() {
  for (auto &p : players) {
    p.active = true;
  }
  // new_round_timer = -1;
}

GameManager::GameManager(uint32_t room_id,
                         std::vector<PlayerIdState> playerInfos)
    : room_id(room_id) {
  NewGame(playerInfos);
}

void to_json(json &j, const GameManager &gm) {
  j = json{
      {"room_id", gm.room_id},
      {"asteroids", gm.asteroids},
      {"players", gm.players},
      {"bulelts", gm.bullets},
      {"winner_player_id", gm.winner_player_id},
      // {"_spawnerTime", gm._spawnerTime},
      // {"startRoundTime", gm.startRoundTime},
      // {"endRoundTime", gm.endRoundTime} ,
      // {"game_start_time", gm.game_start_time}
  };
  // asteroids, players, bullets omitted
}

void from_json(const json &j, GameManager &gm) {
  j.at("room_id").get_to(gm.room_id);
  j.at("asteroids").get_to(gm.asteroids);
  j.at("players").get_to(gm.players);
  j.at("bulelts").get_to(gm.bullets);
  j.at("winner_player_id").get_to(gm.winner_player_id);
}
