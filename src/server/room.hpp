#pragma once
#include "gameManager.hpp"
#include "lobbyManager.hpp"
#include <thread>

struct Room {
  size_t room_id;
  GameManager gameManager;
  LobbyManager lobbyManager;
  std::thread gameThread;

  Room();
  ~Room();
};
