#include "asteroid.hpp"
#include "bullet.hpp"
#include "gameManager.hpp"
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

  // Connection
  int connect_to(const char *host, const char *port);
  void disconnect();
  void set_socket_timeout(time_t seconds);

  // Player
  uint32_t get_new_client_id();
  int vote_ready();
  bool send_movement(Vector2 direction);
  bool shoot_bullets();

  // Room
  std::vector<Room> get_rooms();
  bool join_room(uint32_t room_id);
  void leave_room();

  // Fetching data from server
  Room fetch_room_state();
  Room fetch_room_state(uint32_t fetch_room_id);
  GameManager fetch_game_state();
  std::vector<Player> fetch_players();
  std::vector<Asteroid> fetch_asteroids();
  std::vector<Bullet> fetch_bullets();

  void handle_end_round();
  bool handle_connection_check();
};
