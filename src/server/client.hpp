#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
  int fd;
  size_t client_id = 0;
  size_t player_id = 0;
  size_t room_id = 0;
};
