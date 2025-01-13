#pragma once
#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameManager.hpp"
#include "lockingQueue.hpp"
#include "networkEvents.hpp"
#include "player.hpp"
#include "room.hpp"
#include <atomic>
#include <cstdint>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

struct ClientNetworkManager {
  int mainfd;
  int epollfd;
  uint32_t client_id = 0;

  inline uint8_t get_networks_idx(std::atomic_uint8_t &draw_idx) {
    return !draw_idx.load();
  }

  std::array<GameManager, 2> &gameManagersPair;
  std::atomic_uint8_t &game_manager_draw_idx;
  inline GameManager &gameManager() {
    return gameManagersPair.at(get_networks_idx(game_manager_draw_idx));
  }
  void flip_game_manager();

  std::array<std::map<uint32_t, Room>, 2> &roomsMapPair;
  std::atomic_uint8_t &rooms_draw_idx;
  inline std::map<uint32_t, Room> &rooms() {
    return roomsMapPair.at(get_networks_idx(rooms_draw_idx));
  }
  void flip_rooms();

  std::array<Room, 2> &joinedRoomPair;
  std::atomic_uint8_t &joined_room_draw_idx;
  Room &joinedRoom() {
    return joinedRoomPair.at(get_networks_idx(joined_room_draw_idx));
  }
  void flip_joined_room();

  std::atomic_bool _stop = false;
  std::thread main_thread;
  LockingQueue<std::function<bool(void)>> todo;
  const char *connected_to_host;
  const char *connected_over_port;

  ClientNetworkManager(const char *host, const char *port,
                       std::array<GameManager, 2> &gameManagersPair,
                       std::atomic_uint8_t &game_manager_draw_idx,
                       std::array<std::map<uint32_t, Room>, 2> &roomsMapPair,
                       std::atomic_uint8_t &rooms_map_draw_idx,
                       std::array<Room, 2> &joinedRoomPair,
                       std::atomic_uint8_t &joined_room_draw_idx);
  ~ClientNetworkManager();

  // NOTE: If a function returns:
  // - bool:
  //  - true is ok
  //  - false is some kind of error
  // - int:
  //  - value is propagated from C sockets API function
  //
  // NOTE: If a function accepts non constant reference (&) to an object then it
  // is the return value and will be modified. In that case check the return
  // value for status.

  // Main thread
  void perform_network_actions();
  void handle_network_event(uint32_t network_event);

  // Connection
  int connect_to(const char *host, const char *port);
  int disconnect();
  int set_socket_timeout(time_t seconds);
  bool reconnect(const char *host, const char *port, uint32_t client_id);

  // Player
  bool get_new_client_id(uint32_t &new_client_id);
  bool vote_ready(std::vector<PlayerIdState> &players);
  bool send_movement(Vector2 position, Vector2 velocity, float rotation);
  bool shoot_bullet();
  bool readGameState();

  // Room
  bool get_rooms(std::map<uint32_t, Room> &rooms);
  bool join_room(uint32_t join_room_id, uint32_t &player_id);
  bool leave_room();

  // Fetching data from server
  bool fetch_room_state();
  bool fetch_room_state(Room &room);
  bool fetch_room_state(uint32_t fetch_room_id, Room &room);
  bool fetch_game_state(GameManager &gameManager);
  bool fetch_players(std::vector<Player> &players);
  bool fetch_asteroids(std::vector<Asteroid> &asteroids);
  bool fetch_bullets(std::vector<Bullet> &bullets);

  bool handle_end_round(uint32_t &winner_player_id);
  bool handle_connection_check();
  void read_update_players();
};
