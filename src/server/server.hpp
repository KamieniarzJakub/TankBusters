#pragma once
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "client.hpp"
#include "gameManager.hpp"
#include "lockingQueue.hpp"
#include "networkEvents.hpp"
#include "room.hpp"

struct GameRoom {
  Room room;
  GameManager gameManager;
  std::vector<uint32_t> clients;
  std::mutex gameRoomMutex;
  // FIXME: update game state during game
  std::thread thread;
};

struct Server {
  int mainfd;

  std::thread connection_thread;

  std::atomic_bool _stop = false;

  std::mutex games_mutex;
  std::map<uint32_t, GameRoom> games;
  std::atomic_uint32_t _next_game_id = 1;

  std::mutex clients_mutex;
  std::map<uint32_t, Client> clients;
  std::atomic_uint32_t _next_client_id = 1;

  std::map<uint32_t, LockingQueue<std::function<void(Client c)>>> todos;

  Server(in_port_t main_port);
  ~Server();

  void listen_for_connections();
  std::map<uint32_t, Room> get_available_rooms();
  void new_client(Client client);
  bool delete_client(size_t client_id);
  Client *find_client(size_t client_id);
  uint32_t get_next_available_player_id(GameRoom &gr);

  void handle_connection(int client_fd);
  void handle_network_event(Client &client, uint32_t networkEvent);
  void client_error(Client &client);
  void disconnect_client(Client &client);
  void serverSetEvent(Client &client, NetworkEvents event);
  void serverSetStreamEvent(Client &client, NetworkEvents event);

  uint32_t handleGetClientId(int client_fd);
  void handleGetRoomList(Client &client);
  void handleVoteReady(Client &client);
  void handlePlayerMovement(Client &client);
  void handleShootBullet(Client &client);
  void handleJoinRoom(Client &client);
  void handleLeaveRoom(Client &client);
  void handleUpdateGameState(Client &client);
  void handleUpdateRoomState(Client &client);
  void handleUpdatePlayers(Client &client);
  void handleUpdateAsteroids(Client &client); // all asteroids
  void handleUpdateAsteroids(Client &client,
                             const std::vector<uint32_t> &asteroid_ids,
                             const std::vector<Asteroid> &asteroids);
  void handleUpdateBullets(Client &client);
  void handleBulletDestroyed(Client &client, uint32_t bullet_id);
  void handlePlayerDestroyed(Client &c1, uint32_t player_id);
  bool sendCheckConnection(Client &client);

  bool sendUpdateRoomState(Client &client);
  void new_game(const Room r);
  void sendNewGameSoon(Client &client, uint32_t when);
  void handleSpawnAsteroid(Client &client, Asteroid a, uint32_t id);
  void handleAsteroidDestroyed(Client &client, uint32_t asteroid_id);
  void invalid_network_event(Client &client, uint32_t event);
  void restart_timer(GameRoom &gr, std::vector<uint32_t> &last_clients);
};
