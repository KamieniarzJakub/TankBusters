#pragma once
#include "gameStatus.hpp"
#include "player.hpp"
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

struct PlayerShortInfo {
  uint32_t player_id = -1;
  PlayerInfo state = PlayerInfo::NONE;
};

struct Room {
  uint32_t room_id = 0;
  std::vector<PlayerShortInfo> players;
  // uint32_t players = 0;
  // uint32_t ready_players = 0;
  GameStatus status = GameStatus::LOBBY;
};

// Room &at_room_id(std::vector<Room> &rooms, uint32_t room_id);
uint32_t get_X_players(const std::vector<PlayerShortInfo> &ps, PlayerInfo pi);

void to_json(json &j, const Room &r);
void from_json(const json &j, Room &r);

void to_json(json &j, const PlayerShortInfo &r);
void from_json(const json &j, PlayerShortInfo &r);
