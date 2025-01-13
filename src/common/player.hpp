#pragma once
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>

using json = nlohmann::json;
enum PlayerInfo { NONE = 0, NOT_READY = 1, READY = 2 };

NLOHMANN_JSON_SERIALIZE_ENUM(PlayerInfo, {{NONE, nullptr},
                                          {NOT_READY, "NOT_READY"},
                                          {READY, "READY"}})

struct Player {
  bool active = false;
  Vector2 position;
  Vector2 velocity;
  float rotation;
  Color player_color;
  uint32_t player_id = 0;
};

Player AddPlayer(int i);

void UpdatePlayer(Player &player, float frametime);

bool Shoot();

Vector2 GetPlayerSpawnPosition(int i);
void REALLYJUSTUPDATEPLAYER(Player &player, float frametime);

void to_json(json &j, const Player &p);
void from_json(const json &j, Player &p);
