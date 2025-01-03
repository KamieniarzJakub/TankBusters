#include "server.hpp"
#include "jsonutils.hpp"
#include "networkUtils.hpp"
#include "player.hpp"
#include <errno.h>
#include <error.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
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
  client.player_id = read_uint32(client.fd);
  // std::cout << client.player_id << std::endl;
  if (client.player_id == 0) {
    write_uint32(client.fd, _next_client_id++);
  }

  auto rooms = std::vector<Room>(5, Room{0});
  // std::cout << json(rooms).dump() << std::endl;
  auto json_rooms = json(rooms);
  write_json(client.fd, json_rooms);

  shutdown(client.fd, SHUT_RDWR);
  close(client.fd);
}

std::vector<Room> Server::get_available_rooms() {
  std::vector<Room> rs;
  for (Room &r : rooms) {
    if ((r.gameManager.GetConnectedPlayers(PlayerConnection::None) +
         r.gameManager.GetConnectedPlayers(PlayerConnection::Disconnected))) {
      rs.push_back(r);
    }
  }
  return rs;
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
