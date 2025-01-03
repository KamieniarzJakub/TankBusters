#pragma once
#include "gameManager.hpp"
#include <thread>

struct Room {
  size_t room_id;
  GameManager gameManager;
  std::thread gameThread;

  Room();
  ~Room();
};
