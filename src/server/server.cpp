#include "server.hpp"
#include "constants.hpp"
#include "networkUtils.hpp"
#include <errno.h>
#include <error.h>
#include <iostream>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

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
}

void Server::listen_for_connections() {
  while (!this->_stop) {
    sockaddr_in clientAddr{};
    socklen_t socklen = sizeof(clientAddr);

    int client_fd = accept(fd, (sockaddr *)&clientAddr, &socklen);

    new_client(client_fd);
  }
}

void Server::handle_connection(Client client) {
  char buf;
  read(client.fd, &buf, 1);
  std::cout << buf << std::endl;
  // long room_id = ntohl(buf);
  // std::string m = std::to_string(room_id);
  // write(1, &m, sizeof(m));
}

size_t Server::get_available_rooms(size_t *room_ids) {
  room_ids = (size_t *)calloc(rooms.size(), sizeof(size_t));
  size_t n = 0;
  for (size_t i = 0; i < rooms.size(); i++) {
    if (rooms[i].gameManager.GetReadyPlayers() >= Constants::PLAYERS_MAX)
      continue;
    room_ids[i] = htonl(rooms[i].room_id);
    n++;
  }
  return n;
}

void Server::new_client(int client_fd) {
  std::thread(&Server::handle_connection, this, Client{client_fd}).detach();
}

Client *Server::find_client(size_t client_id) {
  for (Client *client = &clients.front(); client != &clients.back(); client++) {
    if (client->player_id == client_id) {
      return client;
    }
  }
  return nullptr;
}

void Server::delete_client(size_t client_id) {
  Client *c = find_client(client_id);
  if (c == nullptr)
    return;

  int res = shutdown(c->fd, SHUT_RDWR);
  if (res) {
    error(0, errno, "Failed shutdown for client: %lu", c->player_id);
  }
  res = close(c->fd);
  if (res) {
    error(0, errno, "Failed socket close for client: %lu", c->player_id);
  }
}
