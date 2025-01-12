#pragma once
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>

using json = nlohmann::json;
enum PlayerInfo { NONE = 0, NOT_READY = 1, READY = 2 };

NLOHMANN_JSON_SERIALIZE_ENUM(PlayerInfo, {{NONE, nullptr},
                                          {NOT_READY, "NOT_READY"},
                                          {READY, "READY"}})

enum PlayerConnection {
  None = 0,
  Disconnected = 1,
  PoorConnection = 2,
  Connected = 3
};

NLOHMANN_JSON_SERIALIZE_ENUM(PlayerConnection,
                             {{None, nullptr},
                              {Disconnected, "Disconnected"},
                              {PoorConnection, "PoorConnection"},
                              {Connected, "Connected"}})

struct Player {
  PlayerInfo state = PlayerInfo::NONE; // FIXME: delete field
  bool active = false;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  Color player_color;
  PlayerConnection connection_state = PlayerConnection::None;
  uint32_t player_id = 0;
};

Player AddPlayer(int i);

void UpdatePlayer(Player &player, float frametime);

bool Shoot();

Vector2 GetPlayerSpawnPosition(int i);

void to_json(json &j, const Player &p);
void from_json(const json &j, Player &p);
