#include "server.hpp"
#include "networkUtils.hpp"
#include <errno.h>
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>

Server::Server(in_port_t port) {

  this->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (this->fd == -1)
    error(1, errno, "socket failed");

  setReuseAddr(this->fd);

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons((short)port);
  serverAddr.sin_addr = {INADDR_ANY};

  int res = bind(this->fd, (sockaddr *)&serverAddr, sizeof(serverAddr));
  if (res)
    error(1, errno, "bind failed");

  res = listen(this->fd, 1);
  if (res)
    error(1, errno, "listen failed");

  this->epollfd = epoll_create1(0);
  if (epollfd == -1)
    error(1, errno, "epoll_create1");

  epoll_event ee{EPOLLIN, {0}};
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ee);
  this->connection_thread = std::thread(&Server::listen_for_connections, this);
}

Server::~Server() {
  if (this->connection_thread.joinable())
    this->connection_thread.join();
}

void Server::listen_for_connections() {
  while (!this->_stop) {
    int res = epoll_wait(epollfd, events, Constants::MAX_EPOLL_EVENTS, -1);
    if (res == -1 && errno != EINTR) {
      error(0, errno, "epoll_wait failed");
    }
    for (int i = 0; i < res; i++) {
    }
  }
}
