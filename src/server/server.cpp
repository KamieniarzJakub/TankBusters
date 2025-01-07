#include "server.hpp"
#include "jsonutils.hpp"
#include "networkUtils.hpp"
#include "player.hpp"
#include "room.hpp"
#include <cstdint>
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

  this->sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (this->sfd == -1)
    error(1, errno, "socket failed");

  setReuseAddr(this->sfd);

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons((short)port);
  serverAddr.sin_addr = {INADDR_ANY};

  int res = bind(this->sfd, (sockaddr *)&serverAddr, sizeof(serverAddr));
  if (res)
    error(1, errno, "bind failed");

  res = listen(this->sfd, 1);
  if (res)
    error(1, errno, "listen failed");

  this->connection_thread = std::thread(&Server::listen_for_connections, this);
  this->rooms = std::vector<Room>(4, Room()); // FIXME:
  rooms[0].players = 3;
}

Server::~Server() {
  // join worker threads
  if (this->connection_thread.joinable())
    this->connection_thread.join();

  // close network resources
  int res = shutdown(sfd, SHUT_RDWR);
  if (res)
    error(1, errno, "shutdown fd failed");
  res = close(sfd);
  if (res)
    error(1, errno, "close fd failed");
}

void Server::listen_for_connections() {
  while (!this->_stop) {
    sockaddr_in clientAddr{};
    socklen_t socklen = sizeof(clientAddr);

    int client_fd = accept(sfd, (sockaddr *)&clientAddr, &socklen);

    new_client(client_fd);
  }
}

void Server::handle_connection(Client client) {
  while (true) {
    uint32_t event = read_uint32(client.fd);
    handle_network_event(client, event);
  }
}

void Server::handle_network_event(Client &client, uint32_t event) {
  switch (event) {
  case NetworkEvents::NoEvent:
    break;
  case NetworkEvents::GetPlayerId: {
    uint32_t client_id = read_uint32(client.fd);
    // 0 new client, 0> existing client reconnecting
    client.player_id = client_id;
    write_uint32(client.fd, NetworkEvents::GetPlayerId);
    write_uint32(client.fd, _next_client_id++);
    break;
  }
  case NetworkEvents::GetRoomList: {
    write_uint32(client.fd, NetworkEvents::GetRoomList);
    auto j_rooms = json(rooms);
    write_json(client.fd, j_rooms);
    break;
  }
  default:
    break;
  }
}

void Server::disconnect_client(Client &client) {
  shutdown(client.fd, SHUT_RDWR);
  close(client.fd);
}

std::vector<Room> Server::get_available_rooms() {
  std::vector<Room> rs;
  for (Room &r : rooms) {
    // if ((r.gameManager.GetConnectedPlayers(PlayerConnection::None) +
    //      r.gameManager.GetConnectedPlayers(PlayerConnection::Disconnected)))
    //      {
    rs.push_back(r);
    // }
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
