#pragma once
#include "gameManager.hpp"
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Room {
  size_t room_id;
  GameManager gameManager;
  std::thread gameThread;

  Room();
  ~Room();
};

void to_json(json &j, const Room &r);
void from_json(const json &j, Room &r);
