#pragma once
#include "gameManager.hpp"
#include <thread>

struct Room {
  GameManager gameManager;
  std::thread gameThread;
  std::vector<std::thread> playerThreads;
};
