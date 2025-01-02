#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

struct Client {
  int fd;
  size_t player_id;
  size_t room_id;
  std::thread thread;
  Client(int fd);
  ~Client();

  // void handle_connection();
};
