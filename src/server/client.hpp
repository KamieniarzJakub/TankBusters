#pragma once
#include <ctime>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
  int fd_main;
  int fd_stream;
  size_t client_id = 0;
  size_t player_id = 0;
  size_t room_id = 0;
  std::time_t last_response;

  const char *signature();
};
