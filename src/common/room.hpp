#pragma once
#include "gameStatus.hpp"
#include "player.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Room {
  uint32_t room_id = 0;
  uint32_t players = 0;
  uint32_t ready_players = 0;
  GameStatus status = GameStatus::LOBBY;
};

void to_json(json &j, const Room &r);
void from_json(const json &j, Room &r);
