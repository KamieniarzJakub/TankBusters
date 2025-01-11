#pragma once
#include "lockingQueue.hpp"
#include <ctime>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
  int fd_main;
  size_t client_id = 0;
  size_t player_id = 0;
  size_t room_id = 0;
  std::time_t last_response;
  int epd;
  LockingQueue<std::function<bool(void)>> todo;

  const char *signature();
};
