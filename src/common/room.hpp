#pragma once
#include "gameStatus.hpp"
#include "player.hpp"
#include <nlohmann/json.hpp>
#include <vector>

using json = nlohmann::json;

struct PlayerIdState {
  uint32_t player_id = -1;
  PlayerInfo state = PlayerInfo::NONE;
};

struct Room {
  uint32_t room_id = 0;
  std::vector<PlayerIdState> players;
  GameStatus status = GameStatus::LOBBY;
  std::string name;
};

// Room &at_room_id(std::vector<Room> &rooms, uint32_t room_id);
uint32_t get_X_players(const std::vector<PlayerIdState> &ps, PlayerInfo pi);

void to_json(json &j, const Room &r);
void from_json(const json &j, Room &r);

void to_json(json &j, const PlayerIdState &r);
void from_json(const json &j, PlayerIdState &r);
