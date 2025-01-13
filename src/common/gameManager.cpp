#include "gameManager.hpp"
#include "constants.hpp"
#include "player.hpp"
#include <chrono>

using namespace std::chrono;

GameManager::GameManager() {
  NewGame(std::vector<PlayerShortInfo>(Constants::PLAYERS_MAX));
}

GameManager::~GameManager() {}

void GameManager::NewGame(std::vector<PlayerShortInfo> playerInfos) {
  asteroids = std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid());
  players.clear();
  players.reserve(Constants::PLAYERS_MAX);
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    players.push_back(AddPlayer(i));
    players.at(i).active = playerInfos.at(i).state != PlayerInfo::NONE;
  }
  bullets = std::vector<Bullet>(
      Constants::BULLETS_PER_PLAYER * Constants::PLAYERS_MAX, Bullet());
  status = GameStatus::LOBBY;
  _spawnerTime = time_point<steady_clock>(0s);
  _alive_players = GetReadyPlayers(playerInfos);
  startRoundTime = steady_clock::now();
  endRoundTime = time_point<steady_clock>(0s);
}

void GameManager::UpdateStatus() {
  if (_alive_players > 1) {
    status = GameStatus::GAME;
    endRoundTime = steady_clock::now();
  } else if (endRoundTime.time_since_epoch().count() > 0) {
    status = GameStatus::END_OF_ROUND;
    if (steady_clock::now() - endRoundTime >= Constants::NEW_ROUND_WAIT_TIME)
      endRoundTime = time_point<steady_clock>(0s);
  } else {
    status = GameStatus::LOBBY;
  }
}

void GameManager::UpdatePlayers(duration<double> frametime) {
  exit(1); // DO NOT USE
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    CheckMovementUpdatePlayer(players[i], frametime);
    if (players[i].active && Shoot()) {
      AddBullet(players[i]);
    }
  }
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

// void GameManager::UpdateGameServer() { // FIXME: delete inputs
//   UpdateStatus();
//   // TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
//   if (status == GameStatus::GAME) {
//     float frametime = GetFrameTime();
//
//     ManageCollisions();
//
//     UpdatePlayers(frametime);
//     UpdateBullets(frametime);
//     UpdateAsteroids(frametime);
//
//     AsteroidSpawner(GetTime());
//   } else {
//     if (UpdateLobbyStatus()) {
//       NewGame(GetReadyPlayers());
//       RestartLobby();
//     }
//     UpdatePlayersLobby();
//   }
// }

bool GameManager::AsteroidSpawner() {
  bool updated = false;
  auto now = std::chrono::steady_clock::now();
  if (now > _spawnerTime + Constants::ASTEROID_SPAWN_DELAY) {
    TraceLog(LOG_DEBUG, "ASTEROID SPAWNER");
    _spawnerTime = now;
    AddAsteroid();
    updated = true;
  }
  return updated;
}

void GameManager::ManageCollisions() {
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
          bullets[k].active = false;
          asteroids[i].active = false;
          SplitAsteroid(asteroids[i].position, asteroids[i].velocity,
                        float(asteroids[i].size) /
                            Constants::ASTEROID_SPLIT_LOSS);
          break;
        }

        for (int l = 0; l < Constants::PLAYERS_MAX; l++) {
          if (j == l || !players[l].active)
            continue;

          if (CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE,
                                    players[l].position,
                                    Constants::PLAYER_SIZE / 3.0f)) {
            players[l].active = false;
            _alive_players--;
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
        players[j].active = false;
        _alive_players--;
        asteroids[i].active = false;
        SplitAsteroid(asteroids[i].position, asteroids[i].velocity,
                      float(asteroids[i].size) /
                          Constants::ASTEROID_SPLIT_LOSS);
      }
    }
  }
  // TraceLog(LOG_DEBUG, "Alive players: %d", _alive_players);
}

void GameManager::AddAsteroid() {
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    if (asteroids[i].active)
      continue;
    asteroids[i] = CreateAsteroid();
    return;
  }
  TraceLog(LOG_ERROR, "Failed to create an asteroid - no empty slots left");
}

void GameManager::SplitAsteroid(Vector2 position, Vector2 velocity, int size) {
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
    toSpawn--;
  }

  TraceLog(LOG_ERROR, "Failed to split an asteroid - no empty slots left");
}

bool GameManager::AddBullet(const Player &player) {
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
    const std::vector<PlayerShortInfo> &player_infos) const {
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

bool GameManager::ReturnToRooms() {
  return (status == GameStatus::LOBBY &&
          (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)));
}

void GameManager::RestartLobby() {
  for (auto &p : players) {
    p.active = true;
  }
  // new_round_timer = -1;
}

GameManager::GameManager(uint32_t room_id,
                         std::vector<PlayerShortInfo> playerInfos)
    : room_id(room_id) {
  NewGame(playerInfos);
}

void to_json(json &j, const GameManager &gm) {
  j = json{
      {"room_id", gm.room_id},
      {"status", gm.status},
      {"alive_players", gm._alive_players},
      // {"new_round_timer", gm.new_round_timer}
  };
  // asteroids, players, bullets omitted
}

void from_json(const json &j, GameManager &gm) {
  j.at("room_id").get_to(gm.room_id);
  j.at("status").get_to(gm.status);
  j.at("alive_players").get_to(gm._alive_players);
  // j.at("new_round_timer").get_to(gm.new_round_timer);
}
