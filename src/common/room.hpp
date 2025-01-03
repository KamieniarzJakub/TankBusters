#pragma once
#include "gameManager.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Room {
  size_t room_id;
  GameManager gameManager;
};

void to_json(json &j, const Room &r);
void from_json(const json &j, Room &r);
