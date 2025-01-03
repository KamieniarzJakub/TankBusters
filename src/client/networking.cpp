#include "networking.hpp"
#include "jsonutils.hpp"
#include <cstdint>
#include <errno.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ostream>
#include <room.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using json = nlohmann::json;

void do_stuff(const char *host, const char *port) {
  int status, fd;
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  struct addrinfo *p_addr = nullptr;
  struct addrinfo *c_addr = nullptr;
  status = getaddrinfo(host, port, &hints, &p_addr);
  if (status != 0 || p_addr == nullptr) {
    error(1, errno, "Couldn't resolve hostname or service\n");
  }

  for (c_addr = p_addr; c_addr != nullptr; c_addr = c_addr->ai_next) {

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
  if (c_addr == nullptr) {
    error(1, errno, "Could not connect\n");
  }

  uint32_t client_id = 0;
  write_uint32(fd, client_id);

  client_id = read_uint32(fd);
  std::cout << client_id << std::endl;
  json rooms = read_json(fd);
  std::cout << rooms.dump() << std::endl;

  status = shutdown(fd, SHUT_RDWR);
  // if (status != 0) {
  //   error(1, errno, "Couldn't shutdown the connection properly\n");
  // }
  //
  status = close(fd);
  // if (status != 0) {
  //   error(1, errno, "Couldn't close the socket properly\n");
  // }
}
