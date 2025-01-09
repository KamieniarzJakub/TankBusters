#include "gameManager.hpp"
#include "constants.hpp"
#include "player.hpp"

GameManager::GameManager() { NewGame(); }

GameManager::~GameManager() {}

void GameManager::NewGame(int players_in_game) {
  asteroids = std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid());
  players.clear();
  players.reserve(Constants::PLAYERS_MAX);
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    players.push_back(AddPlayer(i));
  }
  bullets = std::vector<Bullet>(
      Constants::BULLETS_PER_PLAYER * Constants::PLAYERS_MAX, Bullet());
  status = GameStatus::LOBBY;
  _spawnerTime = 0;
  _alive_players = players_in_game;
  startRoundTime = GetTime();
  endRoundTime = -1;
}

void GameManager::UpdateGameStatus() {
  if (_alive_players > 1) {
    status = GameStatus::GAME;
    endRoundTime = GetTime();
  } else if (endRoundTime > 0) {
    status = GameStatus::END_OF_ROUND;
    if (GetTime() - endRoundTime >= Constants::NEW_ROUND_WAIT_TIME)
      endRoundTime = -1;
  } else {
    status = GameStatus::LOBBY;
  }
}

void GameManager::UpdatePlayers(float frametime) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    UpdatePlayer(&players[i], frametime);
    if (players[i].active && Shoot()) {
      AddBullet(players[i], i);
    }
  }
}

void GameManager::UpdateBullets(float frametime) {
  for (int i = 0; i < Constants::PLAYERS_MAX * Constants::BULLETS_PER_PLAYER;
       i++) {
    UpdateBullet(&bullets[i], frametime);
  }
}

void GameManager::UpdateAsteroids(float frametime) {
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    UpdateAsteroid(&asteroids[i], frametime);
  }
}

void GameManager::AsteroidSpawner(double time) {
  if (time > _spawnerTime + Constants::ASTEROID_SPAWN_DELAY) {
    TraceLog(LOG_DEBUG, "ASTEROID SPAWNER");
    _spawnerTime = time;
    AddAsteroid();
  }
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

void GameManager::AddBullet(Player player, int player_number) {
  for (int i = player_number * Constants::BULLETS_PER_PLAYER;
       i < (player_number + 1) * Constants::BULLETS_PER_PLAYER; i++) {
    if (bullets[i].active)
      continue;

    Vector2 offset = Vector2Rotate(Vector2{Constants::PLAYER_SIZE / 2.0f, 0.0f},
                                   player.rotation * DEG2RAD);
    bullets[i] =
        CreateBullet(Vector2Add(player.position, offset), player.rotation);
    return;
  }

  TraceLog(LOG_INFO, "Failed to shoot a bullet - player[%d]: no bullets left",
           player_number);
}

void GameManager::UpdatePlayersLobby() {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    if (IsKeyPressed(KEY_SPACE)) {
      players[i].state = PlayerInfo::READY;
    }
  }
  // TraceLog(LOG_DEBUG, "PLAYERS READY FOR NEW ROUND: %d", ready_players);
}

size_t GameManager::GetReadyPlayers() {
  size_t i = 0;
  for (auto &p : players) {
    if (p.state == PlayerInfo::READY)
      i++;
  }
  return i;
}

bool GameManager::UpdateLobbyStatus() {
  size_t ready_players = GetReadyPlayers();
  if (ready_players > 2) {
    new_round_timer = new_round_timer > 0 ? new_round_timer : GetTime();
    if (new_round_timer > 0 &&
        int(GetTime() - new_round_timer) >= Constants::LOBBY_READY_TIME)
      return true;
  } else
    new_round_timer = -1;
  return false;
}

void GameManager::RestartLobby() {
  for (auto &p : players) {
    p.state = PlayerInfo::NOT_READY;
    p.active = true;
  }
  new_round_timer = -1;
}

GameManager::GameManager(uint32_t room_id, uint32_t player_number)
    : room_id(room_id) {
  NewGame(player_number);
}

size_t GameManager::GetConnectedPlayers(PlayerConnection pc) {
  size_t n = 0;
  for (auto &p : players) {
    if (p.connection_state == pc) {
      n++;
    }
  }
  return n;
}
