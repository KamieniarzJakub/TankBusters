#pragma once
#include "constants.hpp"
#include "room.hpp"
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <threads.h>

struct Server {
  int fd;
  int epollfd;
  std::vector<Room> rooms;
  std::thread connection_thread;
  bool _stop; // I think there is a race condition
  epoll_event events[Constants::MAX_EPOLL_EVENTS];
  Server(in_port_t port);
  ~Server();

  void listen_for_connections();
  size_t create_room();
  size_t new_player(size_t room_id);
};
