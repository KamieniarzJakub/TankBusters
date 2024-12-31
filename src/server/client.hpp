#pragma once
#include "networked.hpp"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

struct Client : public Networked {
  int fd;
  int player_id;
  int room_id;
  void handleNetworkEvent(uint32_t events) override;
  ~Client() override;
};
