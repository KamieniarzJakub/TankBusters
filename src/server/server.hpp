#pragma once
#include "client.hpp"
#include "gameManager.hpp"
#include "networkEvents.hpp"
#include "room.hpp"
#include <atomic>
#include <cstdint>
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <thread>
#include <vector>

struct Server {
  int sfd;
  std::thread connection_thread;

  std::atomic_bool _stop = false;

  std::vector<Room> rooms;
  // size_t _next_room_id = 1;
  std::vector<GameManager> games;
  std::vector<Client> clients;
  std::atomic_size_t _next_client_id = 1;

  Server(in_port_t port);
  ~Server();

  void listen_for_connections();
  std::vector<Room> get_available_rooms();
  void new_client(int fd);
  void delete_client(size_t client_id);
  Client *find_client(size_t client_id);
  void handle_connection(Client client);
  void handle_game_logic();
  void disconnect_client(Client &client);
  void handle_network_event(Client &client, uint32_t networkEvent);
};
