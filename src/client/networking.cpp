#include "networking.hpp"
#include <errno.h>
#include <unistd.h>

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

  char data = 'b';
  write(fd, &data, 1);

  status = shutdown(fd, SHUT_RDWR);
  if (status != 0) {
    error(1, errno, "Couldn't shutdown the connection properly\n");
  }

  status = close(fd);
  if (status != 0) {
    error(1, errno, "Couldn't close the socket properly\n");
  }
}
