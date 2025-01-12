#include "server.hpp"

#include <errno.h>
#include <error.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <ctime>
#include <exception>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <vec2json.hpp>
#include <vector>

#include "client.hpp"
#include "constants.hpp"
#include "gameManager.hpp"
#include "gameStatus.hpp"
#include "jsonutils.hpp"
#include "networkUtils.hpp"
#include "player.hpp"
#include "raylib.h"
#include "room.hpp"

Server::Server(in_port_t main_port) {
  // setup main socket
  this->mainfd = socket(AF_INET, SOCK_STREAM, 0);
  if (this->mainfd == -1) error(1, errno, "main socket failed");

  setReuseAddr(this->mainfd);

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons((short)main_port);
  serverAddr.sin_addr = {INADDR_ANY};

  int res = bind(this->mainfd, (sockaddr *)&serverAddr, sizeof(serverAddr));
  if (res) error(1, errno, "main bind failed");

  res = listen(this->mainfd, 1);
  if (res) error(1, errno, "main listen failed");

  {
    std::lock_guard<std::mutex> gml(games_mutex);
    for (int i = 0; i < 4; i++) {
      uint32_t game_id = _next_game_id++;

      this->games[game_id];  // Create a new game room
      {
        GameRoom &gr = this->games.at(game_id);
        std::lock_guard<std::mutex> grl(gr.gameRoomMutex);
        gr.room = Room{game_id, 0, GameStatus::LOBBY};
        gr.gameManager = GameManager(game_id, 0);
      }
    }
  }

  this->connection_thread = std::thread(&Server::listen_for_connections, this);
}

Server::~Server() {
  // join worker threads
  if (this->connection_thread.joinable()) this->connection_thread.join();

  // close network resources
  int res = shutdown(mainfd, SHUT_RDWR);
  if (res) error(1, errno, "shutdown main fd failed");
  res = close(mainfd);
  if (res) error(1, errno, "close main fd failed");
}

void Server::listen_for_connections() {
  while (!this->_stop) {
    sockaddr_in clientAddrMain{};
    socklen_t socklen = sizeof(clientAddrMain);

    int client_main_fd = accept(mainfd, (sockaddr *)&clientAddrMain, &socklen);
    if (client_main_fd == -1) {
      continue;
    }

    // Client client{client_main_fd};
    std::thread(&Server::handle_connection, this, Client{client_main_fd})
        .detach();
  }
}

void Server::client_error(Client &client) {
  // TODO: Try to recover by flushing network buffers
  disconnect_client(client);
}

void Server::sendCheckConnection(Client &client) {
  // Set socket timeouts
  struct timeval tv;
  size_t tvs = sizeof(tv);
  tv.tv_sec = Constants::CONNECTION_TIMEOUT_MILISECONDS / 1000;
  tv.tv_usec = (Constants::CONNECTION_TIMEOUT_MILISECONDS % 1000) * 1000;
  setsockopt(client.fd_main, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, tvs);
  setsockopt(client.fd_main, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, tvs);

  // Send ping
  uint32_t network_event;
  bool status = write_uint32(client.fd_main, NetworkEvents::CheckConnection);
  if (!status) {
    TraceLog(
        LOG_WARNING,
        "Couldn't write CheckConnection NetworkEvent to client_id=%ld,fd=%d",
        client.client_id, client.fd_main);
    disconnect_client(client);
    std::terminate();  // end only this thread FIXME: kills everything
    return;
  }

  // Expect pong
  status = read_uint32(client.fd_main, network_event);
  if (status) {
    TraceLog(LOG_INFO, "Received NetworkEvent %s from client_id=%ld,fd=%d",
             network_event_to_string(network_event).c_str(), client.client_id,
             client.fd_main);
    handle_network_event(client, network_event);
  }

  // Reset to no timeout
  tv = {};
  setsockopt(client.fd_main, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, tvs);
  setsockopt(client.fd_main, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, tvs);
  std::time(&client.last_response);  // Set last response to current time
}

void Server::handle_connection(Client client) {
  todos[client.client_id];

  const size_t MAX_EVENTS = 2;
  epoll_event ee, events[MAX_EVENTS];
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    TraceLog(LOG_ERROR, "Couldn't instantiate epoll for client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }

  ee.events = EPOLLIN | EPOLLOUT;
  ee.data.fd = client.fd_main;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.fd_main, &ee) == -1) {
    close(epoll_fd);
    TraceLog(LOG_ERROR, "Couldn't instantiate epoll for client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }

  // ee.events = EPOLLOUT;
  // ee.data.fd = client.fd_main;
  // if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.fd_main, &ee) == -1) {
  //   close(epoll_fd);
  //   TraceLog(LOG_ERROR,
  //            "Couldn't instantiate epoll for client_id=%ld,streamfd=%d",
  //            client.client_id, client.fd_main);
  //   disconnect_client(client);
  //   return;
  // }

  std::time(&client.last_response);  // Set last response to current time
  while (client.fd_main > 2) {
    // FIXME: epoll add fd_main write when in game
    // FIXME: epoll rm fd_main write when out of the game
    // try {
    //   const GameRoom &gr = games.at(client.room_id);
    //   if (gr.gameManager.status == GAME) {
    //     if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.fd_main, &ee) == -1) {
    //       continue;
    //     }
    //   }
    //   catch (const std::out_of_range &ex) {
    //     continue;
    //   }
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS,
                          Constants::CONNECTION_TIMEOUT_MILISECONDS);
    if (nfds == -1) {  // EPOLL WAIT ERROR
      close(epoll_fd);
      TraceLog(LOG_ERROR, "Epoll wait error for client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      disconnect_client(client);
      return;
    } else if (nfds == 0) {  // EPOLL WAIT TIMEOUT
      // sendCheckConnection(client); // FIXME:
    }
    for (int n = 0; n < nfds; n++) {
      if (events[n].events & EPOLLOUT) {
        try {
          if (!todos.at(client.client_id).isEmpty()) {
            auto f = todos.at(client.client_id).pop();
            f();
          }

        } catch (const std::out_of_range &ex) {
        }

        // handleUpdateAsteroids(client);
        // handleUpdatePlayers(client);
        // handleUpdateBullets(client);
        // if game/round end
        // FIXME: send END ROUND
        // FIXME: send START ROUND
      } else if (events[n].events & EPOLLIN) {
        uint32_t network_event;
        bool status = read_uint32(client.fd_main, network_event);
        if (status) {
          TraceLog(LOG_INFO,
                   "Received NetworkEvent %s from client_id=%ld,fd=%d",
                   network_event_to_string(network_event).c_str(),
                   client.client_id, client.fd_main);
          handle_network_event(client, network_event);
        } else {
          TraceLog(LOG_WARNING,
                   "Couldn't read NetworkEvent from client_id=%ld,fd=%d",
                   client.client_id, client.fd_main);
          client_error(client);
          break;
        }
      }
    }
  }

  close(epoll_fd);
  disconnect_client(client);
}

void Server::handleGetClientId(Client &client) {
  uint32_t client_id;
  bool status = read_uint32(client.fd_main, client_id);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't read new client_id from client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
  // 0 new client, 0> existing client reconnecting
  client.client_id = 0;
  // NOTE: This is essentially just one if and else
  if (client_id > 0) {
    std::lock_guard<std::mutex> lg(clients_mutex);
    if (auto old_c = clients.find(client_id); old_c != clients.end()) {
      if (old_c->second.fd_main == -1) {  // if old client is disconnected
        client.client_id = old_c->second.client_id;
        client.player_id = old_c->second.fd_main;
        client.room_id = old_c->second.room_id;
        old_c->second.fd_main = client.fd_main;
      }
    }
  }
  if (client.client_id == 0) {  // else
    client.client_id = _next_client_id++;
  }
  {
    std::lock_guard<std::mutex> lg(clients_mutex);
    clients[client_id];
    Client &c = clients.at(client_id);
    c.client_id = client.client_id;
    c.fd_main = client.fd_main;
  }

  status = write_uint32(client.fd_main, NetworkEvents::GetClientId);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write GetClientId NetworkEvent to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  status = write_uint32(client.fd_main, client.client_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't write new client id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
}

void Server::handleGetRoomList(Client &client) {
  bool status = write_uint32(client.fd_main, NetworkEvents::GetRoomList);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write GetRoomList NetworkEvent to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
  auto j_rooms = json(this->get_available_rooms());
  status = write_json(client.fd_main, j_rooms);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send rooms list to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
}

void Server::handleVoteReady(Client &client) {
  bool status = write_uint32(client.fd_main, NetworkEvents::VoteReady);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write GetRoomList NetworkEvent to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
    return;
  }

  uint32_t ready_players = 0;

  try {
    GameRoom &gr = games[client.room_id];
    std::lock_guard<std::mutex> lgm(gr.gameRoomMutex);
    gr.room.ready_players++;
    gr.gameManager.players[client.player_id].state = PlayerInfo::READY;
    ready_players = gr.gameManager.GetReadyPlayers();
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  status = write_uint32(client.fd_main, ready_players);
  if (!status) {
    TraceLog(
        LOG_WARNING,
        "Couldn't write the number of ready clients to client_id=%ld,fd=%d",
        client.client_id, client.fd_main);
    client_error(client);
    return;
  }
}

void Server::handlePlayerMovement(Client &client) {
  Vector2 position, velocity;
  float rotation;
  json movement;
  bool status = read_json(client.fd_main, movement, -1);  // FIXME: MAXSIZE
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of movement");
    client_error(client);
    return;
  }

  try {
    movement.at("position").get_to(position);
    movement.at("velocity").get_to(velocity);
    movement.at("rotation").get_to(rotation);
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into movement");
    client_error(client);
    return;
  }

  // FIXME: value checking

  try {
    GameRoom &gr = games[client.room_id];
    std::lock_guard<std::mutex> lgm(gr.gameRoomMutex);
    Player &p = gr.gameManager.players[client.player_id];
    p.position = position;
    p.velocity = velocity;
    p.rotation = rotation;
  } catch (const std::out_of_range &ex) {
  }
}

void Server::serverSetEvent(Client &client, NetworkEvents event) {
  bool status = write_uint32(client.fd_main, event);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write %s NetworkEvent to client_id=%ld,fd=%d",
             network_event_to_string(event).c_str(), client.client_id,
             client.fd_main);
    client_error(client);
  }
}

void Server::handleShootBullet(Client &client) {
  bool status;

  try {
    GameRoom &gr = games[client.room_id];
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    const Player &p = gr.gameManager.players[client.player_id];
    status = gr.gameManager.AddBullet(p);
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  serverSetEvent(client, NetworkEvents::ShootBullets);

  status = write_uint32(client.fd_main, (uint32_t)status);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't send whether bullet was shot to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
}

void Server::handleJoinRoom(Client &client) {
  bool status;
  uint32_t room_id;
  status = read_uint32(client.fd_main, room_id);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't read room id to join from client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  try {
    {
      auto &gr = games.at(room_id);
      std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
      status = gr.room.players < Constants::PLAYERS_MAX &&
               gr.clients.size() < Constants::PLAYERS_MAX;
      if (status) {
        gr.room.players++;
        gr.clients.push_back(client.client_id);
        gr.gameManager = GameManager(gr.room.room_id, gr.room.players);
      }
    }
    client.room_id = room_id;
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  serverSetEvent(client, NetworkEvents::JoinRoom);

  status = write_uint32(client.fd_main, (uint32_t)status * room_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send joined room id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
}

void Server::handleLeaveRoom(Client &client) {
  bool _ = false;
  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    auto i = gr.clients.begin();
    for (; i != gr.clients.end(); i++) {
      if (*i == client.client_id) {
        // Found, remove client from GameRoom
        gr.clients.erase(i, i);
        _ = true;
        if (gr.room.players > 0) {
          gr.room.players--;
          gr.gameManager = GameManager(gr.room.room_id, gr.room.players);
        }
        break;
      }
    }
  } catch (const std::out_of_range &ex) {
    // Room ID doesn't exist
    _ = false;
    TraceLog(LOG_WARNING,
             "Couldn't make client leave a nonexistent room %lu for "
             "client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
  }

  // broadcast who left
  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    for (auto c : gr.clients) {
      auto &broadcast_client = clients.at(c);
      todos.at(c).push([&]() {
        serverSetEvent(broadcast_client, NetworkEvents::LeaveRoom);
        return write_uint32(broadcast_client.fd_main, client.player_id);
      });
    }
  } catch (const std::out_of_range &ex) {
  }
}

void Server::handleUpdateGameState(Client &client) {
  try {
    json gameState = games.at(client.room_id).gameManager;
    serverSetEvent(client, NetworkEvents::UpdateGameState);
    bool status = write_json(client.fd_main, gameState);
    if (!status) {
      TraceLog(LOG_WARNING,
               "Couldn't send game state json to"
               "client_id=%ld,fd=%d",
               client.room_id, client.client_id, client.fd_main);
      client_error(client);
    }
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_WARNING,
             "Couldn't send game state of nonexistent room "
             "client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
  }
}

void Server::handleUpdateRoomState(Client &client) {
  bool status;
  uint32_t room_id;
  status = read_uint32(client.fd_main, room_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't read room id fetch for client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
    return;
  }

  try {
    json room_json = games.at(room_id).room;
    serverSetEvent(client, NetworkEvents::UpdateRoomState);
    status = write_json(client.fd_main, room_json);
    if (!status) {
      TraceLog(LOG_WARNING, "Couldn't send json of room to client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      client_error(client);
      return;
    }
  } catch (const std::out_of_range &ex) {
    // Invalid room id
    TraceLog(LOG_WARNING, "Invalid room id %lu from client_id=%ld,fd=%d",
             room_id, client.client_id, client.fd_main);
    client_error(client);
    return;
  }
}

void Server::handleUpdatePlayers(Client &client) {
  try {
    bool status;
    json players_json = games.at(client.room_id).gameManager.players;
    serverSetEvent(client, NetworkEvents::UpdatePlayers);
    status = write_json(client.fd_main, players_json);
    if (!status) {
      TraceLog(LOG_WARNING,
               "Couldn't send json of players to client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      client_error(client);
      return;
    }
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_WARNING, "Invalid room id %lu from client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
    return;
  }
}

void Server::handleUpdateAsteroids(Client &client) {
  try {
    bool status;
    json asteroids_json = games.at(client.room_id).gameManager.asteroids;
    serverSetEvent(client, NetworkEvents::UpdateAsteroids);
    status = write_json(client.fd_main, asteroids_json);
    if (!status) {
      TraceLog(LOG_WARNING,
               "Couldn't send json of asteroids to client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      client_error(client);
      return;
    }
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_WARNING, "Invalid room id %lu from client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
    return;
  }
}

void Server::handleUpdateBullets(Client &client) {
  try {
    bool status;
    json bullets_json = games.at(client.room_id).gameManager.bullets;
    serverSetEvent(client, NetworkEvents::UpdateBullets);
    status = write_json(client.fd_main, bullets_json);
    if (!status) {
      TraceLog(LOG_WARNING,
               "Couldn't send json of bullets to client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      client_error(client);
      return;
    }
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_WARNING, "Invalid room id %lu from client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
    return;
  }
}

void Server::handle_network_event(Client &client, uint32_t event) {
  switch ((NetworkEvents)event) {
    case NetworkEvents::NoEvent:
      TraceLog(LOG_WARNING, "NoEvent received from client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      break;
    case NetworkEvents::Disconnect:
      if (!delete_client(client.client_id)) {
        disconnect_client(client);
      }
      break;
    case NetworkEvents::CheckConnection: {
      std::time(&client.last_response);
      break;
    }
    case NetworkEvents::EndRound:
      // Never read by server
      // FIXME: IMPLEMENT SENDING
      break;
    case NetworkEvents::StartRound:
      // Never read by server
      // FIXME: IMPLEMENT SENDING
      break;
    case NetworkEvents::GetClientId:
      std::time(&client.last_response);
      handleGetClientId(client);
      break;
    case NetworkEvents::VoteReady:
      std::time(&client.last_response);
      handleVoteReady(client);
      break;
    case NetworkEvents::PlayerMovement:
      std::time(&client.last_response);
      handlePlayerMovement(client);
      break;
    case NetworkEvents::ShootBullets:
      std::time(&client.last_response);
      handleShootBullet(client);
      break;
    case NetworkEvents::GetRoomList:
      std::time(&client.last_response);
      handleGetRoomList(client);
      break;
    case NetworkEvents::JoinRoom:
      std::time(&client.last_response);
      handleJoinRoom(client);
      break;
    case NetworkEvents::LeaveRoom:
      std::time(&client.last_response);
      handleLeaveRoom(client);
      break;
    case NetworkEvents::UpdateGameState:
      std::time(&client.last_response);
      handleUpdateGameState(client);
      break;
    case NetworkEvents::UpdateRoomState:
      std::time(&client.last_response);
      handleUpdateRoomState(client);
      break;
    case NetworkEvents::UpdatePlayers:
      std::time(&client.last_response);
      handleUpdatePlayers(client);
      break;
    case NetworkEvents::UpdateAsteroids:
      std::time(&client.last_response);
      handleUpdateAsteroids(client);
      break;
    case NetworkEvents::UpdateBullets:
      std::time(&client.last_response);
      handleUpdateBullets(client);
      break;
    default:
      TraceLog(LOG_WARNING,
               "Unknown NetworkEvent received from client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      break;
  }
}

void Server::disconnect_client(Client &client) {
  // FIXME: disconnecting a faulty connection sometimes crashes server
  if (client.fd_main > 2) {
    // No info for client
    todos.erase(client.client_id);

    shutdown(client.fd_main, SHUT_RDWR);
    close(client.fd_main);
    client.fd_main = -1;
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

// FIXME: Replace most of disconnect client with this
bool Server::delete_client(size_t client_id) {
  handleLeaveRoom(clients.at(client_id));

  std::lock_guard<std::mutex> lc(clients_mutex);
  if (auto c = clients.find(client_id); c != clients.end()) {
    int res = shutdown(c->second.fd_main, SHUT_RDWR);
    if (res) {
      TraceLog(LOG_WARNING, "Failed shutdown for client: %lu",
               c->second.client_id);
    }
    res = close(c->second.fd_main);
    if (res) {
      TraceLog(LOG_WARNING, "Failed socket close for client: %lu",
               c->second.client_id);
    }
    c->second.fd_main = -1;
    clients.erase(client_id);
    return true;
  }
  return false;
}
