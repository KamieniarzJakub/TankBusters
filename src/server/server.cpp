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
#include "lockingQueue.hpp"
#include "networkUtils.hpp"
#include "player.hpp"
#include "raylib.h"
#include "room.hpp"

Server::Server(in_port_t main_port) {
  // setup main socket
  this->mainfd = socket(AF_INET, SOCK_STREAM, 0);
  if (this->mainfd == -1)
    error(1, errno, "main socket failed");

  setReuseAddr(this->mainfd);

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons((short)main_port);
  serverAddr.sin_addr = {INADDR_ANY};

  int res = bind(this->mainfd, (sockaddr *)&serverAddr, sizeof(serverAddr));
  if (res)
    error(1, errno, "main bind failed");

  res = listen(this->mainfd, 1);
  if (res)
    error(1, errno, "main listen failed");

  {
    std::lock_guard<std::mutex> gml(games_mutex);
    for (int i = 0; i < 4; i++) {
      uint32_t game_id = _next_game_id++;

      this->games[game_id]; // Create a new game room
      {
        GameRoom &gr = this->games.at(game_id);
        std::lock_guard<std::mutex> grl(gr.gameRoomMutex);
        gr.room =
            Room{game_id, std::vector<PlayerShortInfo>(Constants::PLAYERS_MAX),
                 GameStatus::LOBBY};
        for (uint32_t j = 0; j < Constants::PLAYERS_MAX; j++) {
          gr.room.players.at(j).player_id = j;
        }
        gr.gameManager = GameManager(game_id, gr.room.players);
      }
    }
  }

  this->connection_thread = std::thread(&Server::listen_for_connections, this);
}

Server::~Server() {
  // join worker threads
  if (this->connection_thread.joinable())
    this->connection_thread.join();

  // close network resources
  int res = shutdown(mainfd, SHUT_RDWR);
  if (res)
    error(1, errno, "shutdown main fd failed");
  res = close(mainfd);
  if (res)
    error(1, errno, "close main fd failed");
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
    std::terminate();
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
  std::time(&client.last_response); // Set last response to current time
}

void Server::handle_connection(Client client) {

  const size_t MAX_EVENTS = 2;
  epoll_event ee, events[MAX_EVENTS];
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    TraceLog(LOG_ERROR, "Couldn't epoll_create for client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
  client.epd = epoll_fd;
  TraceLog(LOG_INFO, "Epoll_create %d for client_id=%ld,fd=%d", epoll_fd,
           client.client_id, client.fd_main);

  ee.events = EPOLLIN;
  ee.data.fd = client.fd_main;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.fd_main, &ee) == -1) {
    close(epoll_fd);
    TraceLog(LOG_ERROR,
             "Couldn't instantiate EPOLLIN for main_fd for client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }

  std::time(&client.last_response); // Set last response to current time
  while (client.fd_main > 2) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS,
                          Constants::CONNECTION_TIMEOUT_MILISECONDS);
    if (nfds == -1) { // EPOLL WAIT ERROR
      close(epoll_fd);
      TraceLog(LOG_ERROR, "Epoll wait error for client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      disconnect_client(client);
      return;
    } else if (nfds == 0) { // EPOLL WAIT TIMEOUT
      // sendCheckConnection(client); // FIXME:
    }
    for (int n = 0; n < nfds; n++) {
      if (events[n].data.fd == client.fd_main) {
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
      } else if (events[n].data.fd == client.todo_fd) {
        auto f = todos.at(client.client_id).pop();
        f(client);

        // handleUpdateAsteroids(client);
        // handleUpdatePlayers(client);
        // handleUpdateBullets(client);
        // if game/round end
        // FIXME: send END ROUND
        // FIXME: send START ROUND
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
  // if (client_id > 0) {
  //   std::lock_guard<std::mutex> lg(clients_mutex);
  //   if (auto old_c = clients.find(client_id); old_c != clients.end()) {
  //     if (old_c->second.fd_main == -1) { // if old client is disconnected
  //       client.client_id = old_c->second.client_id;
  //       client.player_id = old_c->second.fd_main;
  //       client.room_id = old_c->second.room_id;
  //       old_c->second.fd_main = client.fd_main;
  //     }
  //   }
  // }
  if (client.client_id == 0) { // else
    client.client_id = _next_client_id++;
  }
  {
    std::lock_guard<std::mutex> lg(clients_mutex);
    clients[client_id];
    Client &c = clients.at(client_id);
    c.client_id = client.client_id;
    c.fd_main = client.fd_main;
  }

  TraceLog(LOG_ERROR, "Creating queue for client_id=%ld,fd=%d",
           client.client_id, client.fd_main);

  TraceLog(LOG_ERROR, "EPD=%d for client_id=%ld,fd=%d", client.epd,
           client.client_id, client.fd_main);
  auto &todo = todos[client.client_id]; // Create todo queue and add to epoll
  epoll_event ee;
  ee.events = EPOLLIN;
  ee.data.fd = todo.get_event_fd();
  if (epoll_ctl(client.epd, EPOLL_CTL_ADD, todo.get_event_fd(), &ee) == -1) {
    close(client.epd);
    TraceLog(
        LOG_ERROR,
        "Couldn't instantiate EPOLLIN for eventfd=%d for client_id=%ld,fd=%d",
        todo.get_event_fd(), client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
  client.todo_fd = todo.get_event_fd();

  TraceLog(LOG_ERROR, "Added queue for client_id=%ld,fd=%d", client.client_id,
           client.fd_main);

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

  try {
    GameRoom &gr = games.at(client.room_id);
    json player_short_infos_json;
    {
      std::lock_guard<std::mutex> lgm(gr.gameRoomMutex);
      // client.player_id = get_next_available_player_id(gr);
      gr.room.players.at(client.player_id).state = PlayerInfo::READY;
      gr.gameManager.players[client.player_id].active = true;
      player_short_infos_json = gr.room.players;
    }
    status = write_json(client.fd_main, player_short_infos_json);
    if (!status) {
      TraceLog(LOG_WARNING,
               "Couldn't send json of players to client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      client_error(client);
      return;
    }
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    for (auto c : gr.clients) {
      if (c == client.client_id) {
        continue;
      }
      todos.at(c).push([&](Client c1) {
        serverSetEvent(c1, NetworkEvents::UpdateRoomState);
        return write_uint32(c1.fd_main, client.player_id);
      });
    }
  } catch (const std::out_of_range &ex) {
  }
}

void Server::handlePlayerMovement(Client &client) {
  Vector2 position, velocity;
  float rotation;
  json movement;
  bool status = read_json(client.fd_main, movement, -1); // FIXME: MAXSIZE
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
    GameRoom &gr = games.at(client.room_id);
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
    GameRoom &gr = games.at(client.room_id);
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
  uint32_t read_room_id;
  status = read_uint32(client.fd_main, read_room_id);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't read room id to join from client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }
  TraceLog(LOG_INFO, "Received room id=%lu to join from client_id=%lu,fd=%d",
           read_room_id, client.client_id, client.fd_main);

  try {
    {
      auto &gr = games.at(read_room_id);
      std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
      status = gr.room.status == GameStatus::LOBBY;
      if (status) {
        auto player_id = get_next_available_player_id(gr);
        status = player_id != UINT32_MAX;
        if (status) {
          client.player_id = player_id;
          gr.room.players.at(player_id).state = PlayerInfo::NOT_READY;
          gr.clients.push_back(client.client_id);
          // gr.gameManager = GameManager(read_room_id, gr.room.players); //
          // FIXME:
        }
        TraceLog(LOG_INFO, json(gr.room).dump().c_str());
        TraceLog(LOG_INFO, "Client player id=%lu", client.player_id);
      }
    }
    client.room_id = read_room_id;
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  serverSetEvent(client, NetworkEvents::JoinRoom);

  status = write_uint32(client.fd_main, (uint32_t)status * read_room_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send joined room id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  status = write_uint32(client.fd_main, client.player_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send player id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  try {
    auto except_client_id = client.client_id;
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    std::cout << "Except c id" << except_client_id << std::endl;
    std::cout << "Clients for room " << gr.room.room_id
              << json(gr.clients).dump() << std::endl;
    for (auto c : gr.clients) {
      if (c == except_client_id) {
        continue;
      }

      TraceLog(LOG_INFO, "setting up updating rooms for %lu", c);
      todos.at(c).push([&](Client c1) {
        TraceLog(LOG_INFO, "updating rooms for %lu", c1.client_id);
        return sendUpdateRoomState(c1);
      });
    }
  } catch (const std::out_of_range &ex) {
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
        try {
          gr.room.players.at(client.player_id).state = PlayerInfo::NONE;
          gr.gameManager.players.at(client.player_id).active = false;
        } catch (const std::out_of_range &ex) {
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
    return;
  }

  serverSetEvent(client, NetworkEvents::LeaveRoom);
  bool status = write_uint32(client.fd_main, client.player_id);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't send leaving room confirmation to "
             "client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
  }

  try {
    auto except_client_id = client.client_id;
    auto &gr = games.at(except_client_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    for (auto c : gr.clients) {
      if (c == except_client_id) {
        continue;
      }

      todos.at(c).push([&](Client c1) { return sendUpdateRoomState(c1); });
    }
  } catch (const std::out_of_range &ex) {
  }
  client.player_id = -1;
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
  status = sendUpdateRoomState(client);
  if (!status) {
    client_error(client);
    return;
  }
}

bool Server::sendUpdateRoomState(Client &client) {
  try {
    json room_json = games.at(client.room_id).room;
    serverSetEvent(client, NetworkEvents::UpdateRoomState);
    bool status = write_json(client.fd_main, room_json);
    if (!status) {
      TraceLog(LOG_WARNING, "Couldn't send json of room to client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      client_error(client);
      return false;
    }
    return status;
  } catch (const std::out_of_range &ex) {
    // Invalid room id
    TraceLog(LOG_WARNING, "Invalid room id %lu from client_id=%ld,fd=%d",
             client.room_id, client.client_id, client.fd_main);
    client_error(client);
    return false;
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
  if (client.fd_main > 2) {
    // No info for client
    {
      std::lock_guard<std::mutex> lc(clients_mutex);
      try {
        todos.erase(client.client_id);
      } catch (const std::out_of_range &ex) {
      }
    }

    shutdown(client.fd_main, SHUT_RDWR);
    close(client.fd_main);
    client.fd_main = -1;
  }
}

std::map<uint32_t, Room> Server::get_available_rooms() {
  std::map<uint32_t, Room> rs;
  for (const auto &game : this->games) {
    const auto &r = game.second.room;
    rs[r.room_id] = r;
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
  std::lock_guard<std::mutex> lc(clients_mutex);
  if (auto c = clients.find(client_id); c != clients.end()) {
    handleLeaveRoom(c->second);
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

uint32_t Server::get_next_available_player_id(GameRoom &gr) {
  uint32_t player_id = 0;
  for (PlayerShortInfo &player : gr.room.players) {
    if (player.state == PlayerInfo::NONE) {
      player.state = PlayerInfo::NOT_READY;
      TraceLog(LOG_INFO, "Next player id: %lu", player_id);
      return player_id;
    }
    player_id++;
  }

  return -1;
}
