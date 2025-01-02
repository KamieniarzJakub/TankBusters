#include "server.hpp"
#include "constants.hpp"
#include "networkUtils.hpp"
#include <errno.h>
#include <error.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

enum State { NEW_CONNECTION, REQUEST_AVAILABLE_ROOMS, CONNECT_TO_ROOM, OTHER };

struct Payload {
  State state;
  void *data;
  size_t len;

  Payload(State state, void *data, size_t len)
      : state(state), data(data), len(len) {}
  ~Payload() { free(data); }
};

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

  epoll_event ee{EPOLLIN, {}};
  ee.data.ptr = new Payload{NEW_CONNECTION, 0, 0};
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ee);
  this->connection_thread = std::thread(&Server::listen_for_connections, this);
}

Server::~Server() {
  // join worker threads
  if (this->connection_thread.joinable())
    this->connection_thread.join();

  // close network resources
  int res = shutdown(fd, SHUT_RDWR);
  if (res)
    error(1, errno, "shutdown fd failed");
  res = close(fd);
  if (res)
    error(1, errno, "close fd failed");
  res = close(epollfd);
  if (res)
    error(1, errno, "close epollfd failed");
  for (auto &e : events) {
    free(e.data.ptr);
  }
}

void Server::listen_for_connections() {
  while (!this->_stop) {
    int res = epoll_wait(epollfd, events, Constants::MAX_EPOLL_EVENTS, -1);
    if (res == -1 && errno != EINTR) {
      error(0, errno, "epoll_wait failed");
    }
    for (int i = 0; i < res; i++) {
      auto &e = events[i];
      if (e.data.ptr == nullptr)
        continue;

      Payload *p = (Payload *)e.data.ptr;

      if (p->state == NEW_CONNECTION) {
        sockaddr_in clientAddr{};
        socklen_t socklen = sizeof(clientAddr);

        int client_fd =
            accept4(fd, (sockaddr *)&clientAddr, &socklen, SOCK_NONBLOCK);
        epoll_event ee{EPOLLIN, {}};

        Payload *p1 = new Payload{REQUEST_AVAILABLE_ROOMS, new int(client_fd),
                                  sizeof(client_fd)};
        ee.data.ptr = p1;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &ee);
      } else if (p->state == REQUEST_AVAILABLE_ROOMS) {
        size_t *room_ids = nullptr;
        auto n = get_available_rooms(room_ids);
        int *cfd = (int *)p->data;
        write(*cfd, room_ids, n);
      }
      delete p;
    }
  }
}

void Server::handle_connection(int client_fd) {
  uint32_t buf;
  ssize_t n = read(client_fd, &buf, sizeof(buf));
  long room_id = ntohl(buf);
}

size_t Server::get_available_rooms(size_t *room_ids) {
  room_ids = (size_t *)calloc(rooms.size(), sizeof(size_t));
  size_t n = 0;
  for (size_t i = 0; i < rooms.size(); i++) {
    if (rooms[i].lobbyManager.players_in_lobby >= Constants::PLAYERS_MAX)
      continue;
    room_ids[i] = htonl(rooms[i].room_id);
    n++;
  }
  return n;
}
