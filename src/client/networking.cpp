#include "networking.hpp"
#include <cstdint>
#include <errno.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ostream>
#include <room.hpp>
#include <string>
#include <sys/types.h>
#include <unistd.h>

using json = nlohmann::json;

void do_stuff(const char *host, const char *port) {
  int status, fd;
  struct addrinfo addr;
  struct addrinfo hints = {0};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  struct addrinfo *p_addr = &addr;
  struct addrinfo *c_addr = NULL;
  status = getaddrinfo(host, port, &hints, &p_addr);
  if (status != 0 || p_addr == NULL) {
    error(1, errno, "Couldn't resolve hostname or service\n");
  }

  for (c_addr = p_addr; c_addr != NULL; c_addr = c_addr->ai_next) {

    fd = socket(p_addr->ai_family, p_addr->ai_socktype, p_addr->ai_protocol);
    if (fd == -1) {
      continue;
    }
    status = connect(fd, p_addr->ai_addr, p_addr->ai_addrlen);
    if (status != -1) {
      break;
    }

    close(fd);
  }

  freeaddrinfo(p_addr);
  if (c_addr == NULL) {
    error(1, errno, "Could not connect\n");
  }

  uint32_t client_id = 0;
  write(fd, &client_id, sizeof(client_id));

  ssize_t rooms_size;
  read(fd, &rooms_size, sizeof(rooms_size));
  rooms_size = ntohl(rooms_size);
  std::cout << rooms_size << std::endl;
  std::string rooms_bson(rooms_size, '\0');
  // std::vector<uint8_t> rooms_bson = std::vector<uint8_t>(rooms_size, 0);
  int r = read(fd, &rooms_bson[0], rooms_size);
  rooms_bson.resize(r);
  std::cout << "r " << r << std::endl;
  // json rooms = json::from_bson(rooms_bson);
  // std::cout << rooms.dump() << std::endl;
  std::cout << rooms_bson << std::endl;

  status = shutdown(fd, SHUT_RDWR);
  if (status != 0) {
    error(1, errno, "Couldn't shutdown the connection properly\n");
  }

  status = close(fd);
  if (status != 0) {
    error(1, errno, "Couldn't close the socket properly\n");
  }
}
