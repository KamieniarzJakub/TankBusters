#include "bullet.hpp"
#include "player.hpp"
#include "room.hpp"
#include <cstdint>
#include <cstdio>
#include <error.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

struct ClientNetworkManager {
  int fd;
  uint32_t client_id = 0;
  uint32_t room_id = 0;

  ClientNetworkManager(const char *host, const char *port);
  ~ClientNetworkManager();

  uint32_t get_new_client_id();
  std::vector<Room> get_rooms();
  void disconnect();
  void set_socket_timeout(time_t seconds);
  int connect_to(const char *host, const char *port);
  // bool join_room(uint32_t room_id);
  // bool send_current_player_state();
  // void fetch_room_state();
  // std::vector<Player> fetch_players();
  // std::vector<Asteroid> fetch_asteroids();
  // std::vector<Bullet> fetch_bullets();
  // bool shoot_bullet();
  // int vote_ready();
};
