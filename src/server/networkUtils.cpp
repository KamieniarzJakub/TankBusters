#include "networkUtils.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <error.h>
#include <sys/socket.h>

uint16_t readPort(char *txt) {
  char *ptr;
  auto port = strtol(txt, &ptr, 10);
  if (*ptr != 0 || port < 1 || (port > ((1 << 16) - 1)))
    error(1, 0, "illegal argument %s", txt);
  return port;
}

void setReuseAddr(int sock) {
  const int one = 1;
  int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (res)
    error(1, errno, "setsockopt failed");
}
