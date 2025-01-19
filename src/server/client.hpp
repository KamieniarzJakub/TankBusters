#pragma once
#include <cstdint>
#include <ctime>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
  int fd_main;
  uint32_t client_id = 0;
  uint32_t player_id = 0;
  uint32_t room_id = 0;
  std::time_t last_response;
  int epd = -1;
  int todo_fd = -1;
  bool good_connection = true;

  const std::string signature();
};
