#pragma once
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "client.hpp"
#include "gameManager.hpp"
#include "networkEvents.hpp"
#include "room.hpp"

struct GameRoom {
  Room room;
  GameManager gameManager;
  std::vector<uint32_t> clients;
  std::mutex gameRoomMutex;
};

struct Server {
  int mainfd;
  int streamfd;

  std::thread connection_thread;

  std::atomic_bool _stop = false;

  std::mutex games_mutex;
  std::map<size_t, GameRoom> games;
  std::atomic_uint32_t _next_game_id = 1;

  std::mutex clients_mutex;
  std::map<size_t, Client> clients;
  std::atomic_uint32_t _next_client_id = 1;

  Server(in_port_t main_port);
  ~Server();

  void listen_for_connections();
  std::vector<Room> get_available_rooms();
  void new_client(Client client);
  bool delete_client(size_t client_id);
  Client *find_client(size_t client_id);

  void handle_connection(Client client);
  void handle_game_logic();
  void handle_network_event(Client &client, uint32_t networkEvent);
  void client_error(Client &client);
  void disconnect_client(Client &client);
  void serverSetEvent(Client &client, NetworkEvents event);
  void serverSetStreamEvent(Client &client, NetworkEvents event);

  void handleSetupStreamConnection(Client &client);
  void handle_stream_network_event(Client &client, uint32_t event);
  void handle_stream_socket(Client &client);
  void handleGetClientId(Client &client);
  void handleGetRoomList(Client &client);
  void handleVoteReady(Client &client);
  void handlePlayerMovement(Client &client);
  void handleShootBullet(Client &client);
  void handleJoinRoom(Client &client);
  void handleLeaveRoom(Client &client);
  void handleUpdateGameState(Client &client);
  void handleUpdateRoomState(Client &client);
  void handleUpdatePlayers(Client &client);
  void handleUpdateAsteroids(Client &client);
  void handleUpdateBullets(Client &client);
  void handleStreamUpdatePlayers(Client &client);
  void handleStreamUpdateBullets(Client &client);
  void handleStreamUpdateAsteroids(Client &client);
  void handleStreamPlayerMovement(Client &client);
  void handleStreamShootBullet(Client &client);
};
