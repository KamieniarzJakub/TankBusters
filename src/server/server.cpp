#include "server.hpp"
#include "gameManager.hpp"
#include "jsonutils.hpp"
#include "networkUtils.hpp"
#include "player.hpp"
#include "raylib.h"
#include "room.hpp"
#include <cstdint>
#include <errno.h>
#include <error.h>
#include <map>
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
  for (int i = 0; i < 4; i++) {
    uint32_t game_id = _next_game_id++;
    this->games[game_id] = GameRoom{
        Room{game_id, 0, GameStatus::LOBBY},
        GameManager(game_id, 0),
    };
  }
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

void Server::client_error(Client &client) {
  // TODO: Try to recover by flushing network buffers
  disconnect_client(client);
}

void Server::handle_connection(Client client) {
  while (true) {
    uint32_t event;
    bool status = read_uint32(client.fd, event);
    if (status) {
      handle_network_event(client, event);
    } else {
      TraceLog(LOG_WARNING,
               "Couldn't read NetworkEvent from client_id=%ld,fd=%d",
               client.client_id, client.fd);
      client_error(client);
      break;
    }
  }
  disconnect_client(client);
}

void Server::handleGetClientId(Client &client) {
  uint32_t client_id;
  bool status = read_uint32(client.fd, client_id);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't read new client_id from client_id=%ld,fd=%d",
             client.client_id, client.fd);
    client_error(client);
  }
  // 0 new client, 0> existing client reconnecting
  client.client_id = 0;
  // NOTE: This is essentially just one if and else
  if (client_id > 0) {
    if (auto old_c = clients.find(client_id); old_c != clients.end()) {
      if (old_c->second.fd == -1) { // if old client is disconnected
        client.client_id = old_c->second.client_id;
        client.player_id = old_c->second.fd;
        client.room_id = old_c->second.room_id;
        old_c->second.fd = client.fd;
      }
    }
  }
  if (client.client_id == 0) { // else
    client.client_id = _next_client_id++;
  }
  status = write_uint32(client.fd, NetworkEvents::GetClientId);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write GetClientId NetworkEvent to client_id=%ld,fd=%d",
             client.client_id, client.fd);
    client_error(client);
  }

  status = write_uint32(client.fd, _next_client_id++);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't write new client id to client_id=%ld,fd=%d",
             client.client_id, client.fd);
    client_error(client);
  }
}

void Server::handleGetRoomList(Client &client) {
  bool status = write_uint32(client.fd, NetworkEvents::GetRoomList);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write GetRoomList NetworkEvent to client_id=%ld,fd=%d",
             client.client_id, client.fd);
    client_error(client);
  }
  auto j_rooms = json(this->get_available_rooms());
  status = write_json(client.fd, j_rooms);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send rooms list to client_id=%ld,fd=%d",
             client.client_id, client.fd);
    client_error(client);
  }
}

void Server::handle_network_event(Client &client, uint32_t event) {
  switch ((NetworkEvents)event) {
  case NetworkEvents::NoEvent:
    TraceLog(LOG_WARNING, "NoEvent received from client_id=%ld,fd=%d",
             client.client_id, client.fd);
    break;
  case NetworkEvents::GetClientId:
    handleGetClientId(client);
    break;
  case NetworkEvents::GetRoomList:
    handleGetRoomList(client);
    break;
  case NetworkEvents::Disconnect:
    delete_client(client.client_id);
  default:
    // FIXME: implement
    break;
  }
}

void Server::disconnect_client(Client &client) {
  if (client.fd > 2) {
    // No info for client
    shutdown(client.fd, SHUT_RDWR);
    close(client.fd);
    client.fd = -1;
  }
}

std::vector<Room> Server::get_available_rooms() {
  std::vector<Room> rs;
  for (const auto &game : this->games) {
    rs.push_back(game.second.room);
  }
  // for (GameManager &g : this->games) {
  //   auto room =
  //       Room{g.room_id,
  //            g.GetConnectedPlayers(PlayerConnection::Connected) +
  //                g.GetConnectedPlayers(PlayerConnection::PoorConnection),
  //            g.status};
  //   rs.push_back(room);
  // }
  return rs;
}

void Server::new_client(int client_fd) {
  std::thread(&Server::handle_connection, this, Client{client_fd}).detach();
}

void Server::delete_client(size_t client_id) {
  if (auto c = clients.find(client_id); c != clients.end()) {
    // TODO: SEND WARNING TO CLIENT
    int res = shutdown(c->second.fd, SHUT_RDWR);
    if (res) {
      TraceLog(LOG_WARNING, "Failed shutdown for client: %lu",
               c->second.player_id);
    }
    res = close(c->second.fd);
    if (res) {
      TraceLog(LOG_WARNING, "Failed socket close for client: %lu",
               c->second.player_id);
    }
    c->second.fd = -1;
    clients.erase(client_id);
  }
}
