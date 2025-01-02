#pragma once
#include "client.hpp"
#include "room.hpp"
#include <atomic>
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <thread>
#include <vector>

struct Server {
  int fd;
  int epollfd;
  std::thread connection_thread;

  std::atomic_bool _stop;
  epoll_event events[Constants::MAX_EPOLL_EVENTS];

  std::vector<Room> rooms;
  // size_t _room_id = 1;
  std::vector<Client> clients;
  // size_t _next_client_id = 1;

  Server(in_port_t port);
  ~Server();

  void listen_for_connections();
  size_t create_room();
  size_t new_player(size_t room_id);
  void handle_connection(int fd);
  size_t authenticate_client(int fd);
  size_t get_available_rooms(size_t *room_ids);
};
