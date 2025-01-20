#pragma once
#include "gameManager.hpp"
#include "lockingQueue.hpp"
#include "networkEvents.hpp"
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
private:
  int mainfd;
  int epollfd;
  uint32_t client_id = 0;

  inline uint8_t get_networks_idx(std::atomic_uint8_t &draw_idx) {
    return !draw_idx.load();
  }

  std::array<GameManager, 2> &gameManagersPair;
  std::atomic_uint8_t &game_manager_draw_idx;

  std::array<std::map<uint32_t, Room>, 2> &roomsMapPair;
  std::atomic_uint8_t &rooms_draw_idx;

  std::array<Room, 2> &joinedRoomPair;
  std::atomic_uint8_t &joined_room_draw_idx;

  std::atomic_uint32_t &player_id;
  std::atomic_bool _stop = false;
  std::thread main_thread;
  const char *connected_to_host;
  const char *connected_over_port;

  // NOTE: If a function returns:
  // - bool:
  //  - true is ok
  //  - false is some kind of error
  // - int:
  //  - value is propagated from C sockets API function
  // Most of the void functions modify object fields
  //
  // NOTE: If a function accepts non constant reference (&) to an object then it
  // is the return value and will be modified. In that case check the return
  // value for status.

  // Main thread
  void perform_network_actions();
  void handle_network_event(uint32_t network_event);

  // NetworkEvents handlers
  void handle_update_game_state();
  void handle_end_round();
  void handle_update_players();
  void handle_update_asteroids();
  void handle_bullet_destroyed();
  void handle_player_destroyed();
  void handle_spawn_asteroid();
  void handle_asteroid_destroyed();
  void handle_new_game_soon();
  void handle_return_to_lobby();
  void handle_vote_ready();
  void handle_player_movement();
  void handle_update_bullets();
  void handle_update_room_state();
  void handle_leave_room();
  void handle_join_room();
  void handle_shoot_bullets();
  void handle_get_room_list();
  void handle_start_round();

public:
  LockingQueue<std::function<void(void)>> todo;

  // Connection
  int connect_to(const char *host, const char *port);
  int disconnect();

  // Player
  bool get_new_client_id(uint32_t &new_client_id);
  bool send_vote_ready();
  bool send_movement(Vector2 position, Vector2 velocity, float rotation);
  bool send_shoot_bullet();

  // Room
  bool get_rooms();
  bool join_room(uint32_t join_room_id);
  bool leave_room();

  inline std::map<uint32_t, Room> &rooms() {
    return roomsMapPair.at(get_networks_idx(rooms_draw_idx));
  }
  void flip_rooms();
  Room &joinedRoom() {
    return joinedRoomPair.at(get_networks_idx(joined_room_draw_idx));
  }
  void flip_joined_room();
  inline GameManager &gameManager() {
    return gameManagersPair.at(get_networks_idx(game_manager_draw_idx));
  }
  void flip_game_manager();

  ClientNetworkManager(const char *host, const char *port,
                       std::array<GameManager, 2> &gameManagersPair,
                       std::atomic_uint8_t &game_manager_draw_idx,
                       std::array<std::map<uint32_t, Room>, 2> &roomsMapPair,
                       std::atomic_uint8_t &rooms_map_draw_idx,
                       std::array<Room, 2> &joinedRoomPair,
                       std::atomic_uint8_t &joined_room_draw_idx,
                       std::atomic_uint32_t &player_id);
  ~ClientNetworkManager();
};
