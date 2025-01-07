#include "room.hpp"
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

  ClientNetworkManager(const char *host, const char *port);
  ~ClientNetworkManager();

  uint32_t get_new_client_id();
  std::vector<Room> get_rooms();
  void disconnect();
  int connect_to(const char *host, const char *port);
};
