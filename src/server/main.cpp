#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <error.h>
#include <iostream>
#include <list>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#include "client.hpp"
#include "constants.hpp"
#include "gameManager.hpp"
#include "networkUtils.hpp"
#include "server.hpp"

int servFd;
int epollFd;
std::unordered_set<Client *> clients;

int main(int argc, char **argv) {
  if (argc != 2)
    error(1, 0, "Need 1 arg (port)");
  auto port = readPort(argv[1]);

  servFd = socket(AF_INET, SOCK_STREAM, 0);
  if (servFd == -1)
    error(1, errno, "socket failed");

  signal(SIGINT, ctrl_c);
  signal(SIGPIPE, SIG_IGN);

  setReuseAddr(servFd);

  sockaddr_in serverAddr{.sin_family = AF_INET,
                         .sin_port = htons((short)port),
                         .sin_addr = {INADDR_ANY}};
  int res = bind(servFd, (sockaddr *)&serverAddr, sizeof(serverAddr));
  if (res)
    error(1, errno, "bind failed");

  res = listen(servFd, 1);
  if (res)
    error(1, errno, "listen failed");

  epollFd = epoll_create1(0);

  epoll_event ee{EPOLLIN, {.ptr = &servHandler}};
  epoll_ctl(epollFd, EPOLL_CTL_ADD, servFd, &ee);

  while (true) {
    if (-1 == epoll_wait(epollFd, &ee, 1, -1) && errno != EINTR) {
      error(0, errno, "epoll_wait failed");
      ctrl_c(SIGINT);
    }
    ((Handler *)ee.data.ptr)->handleEvent(ee.events);
  }
}
