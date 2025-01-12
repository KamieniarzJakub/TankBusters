#pragma once
#include <ctime>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
  int fd_main;
  size_t client_id = 0;
  size_t player_id = 0;
  size_t room_id = 0;
  std::time_t last_response;
  int epd = -1;
  int todo_fd = -1;

  const std::string signature();
};
