#include "server.hpp"

#include <chrono>
#include <errno.h>
#include <error.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <ctime>
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
            Room{game_id, std::vector<PlayerIdState>(Constants::PLAYERS_MAX),
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

bool Server::sendCheckConnection(Client &client) {
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
    return false;
  }

  // Expect pong
  status = read_uint32(client.fd_main, network_event);
  if (status) {
    TraceLog(LOG_INFO, "%s not received from client_id=%ld,fd=%d",
             network_event_to_string(network_event).c_str(), client.client_id,
             client.fd_main);
    disconnect_client(client);
    return false;
  }

  // Reset to no timeout
  tv = {};
  setsockopt(client.fd_main, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, tvs);
  setsockopt(client.fd_main, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, tvs);
  std::time(&client.last_response); // Set last response to current time
  return true;
}

void Server::handle_connection(Client client) {

  const size_t MAX_EVENTS = 3;
  epoll_event ee, events[MAX_EVENTS];
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    TraceLog(LOG_ERROR, "Couldn't epoll_create for client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
  client.epd = epoll_fd;
  // TraceLog(LOG_INFO, "Epoll_create %d for client_id=%ld,fd=%d", epoll_fd,
  //          client.client_id, client.fd_main);

  ee.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
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
    // int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS,
    // Constants::CONNECTION_TIMEOUT_MILISECONDS);
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds == -1) { // EPOLL WAIT ERROR
      close(epoll_fd);
      TraceLog(LOG_ERROR, "Epoll wait error for client_id=%ld,fd=%d",
               client.client_id, client.fd_main);
      disconnect_client(client);
      return;
    } else if (nfds == 0) { // EPOLL WAIT TIMEOUT
      // if (!sendCheckConnection(client)) { // FIXME:
      //   break;
      // }
    }
    for (int n = 0; n < nfds; n++) {
      if (events[n].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
        disconnect_client(client);
        break;
      } else if (events[n].data.fd ==
                 client.fd_main) { // EPOLLIN on client.main_fd
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
      } else if (events[n].data.fd == client.todo_fd) { // Something on queue
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
  client.client_id = client_id;
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
  if (client.client_id == 0) {
    client.client_id = _next_client_id++;
  } else {
    TraceLog(LOG_WARNING, "Non zero client_id received from client");
    client_error(client);
  }
  {
    std::lock_guard<std::mutex> lg(clients_mutex);
    TraceLog(LOG_INFO, "Creating client entry in clients for %lu",
             client.client_id);
    Client &c = clients[client.client_id];
    std::vector<std::vector<uint32_t>> ccc;
    for (auto &m : clients) {
      ccc.push_back(std::vector<uint32_t>{m.first, m.second.player_id});
    }
    TraceLog(LOG_INFO, "Current Clients: %s", json(ccc).dump().c_str());
    c.client_id = client.client_id;
    c.fd_main = client.fd_main;
  }

  TraceLog(LOG_INFO, "Creating queue for client_id=%ld,fd=%d", client.client_id,
           client.fd_main);

  // TraceLog(LOG_INFO, "EPD=%d for client_id=%ld,fd=%d", client.epd,
  //          client.client_id, client.fd_main);
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

  TraceLog(LOG_INFO, "Added queue for client_id=%ld,fd=%d", client.client_id,
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

void Server::sendNewGameSoon(Client &client, uint32_t when) {
  serverSetEvent(client, NetworkEvents::NewGameSoon);
  write_uint32(client.fd_main, when);
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
      if (get_X_players(gr.room.players, READY) >= 2) {
        // TraceLog(LOG_INFO, "2. NEW PLAYER");
        gr.gameManager.game_start_time =
            time(0) + Constants::LOBBY_READY_TIME.count();
        for (auto c : gr.clients) {
          // TraceLog(LOG_INFO, "3. NEW PLAYER");
          todos.at(c).push([&](Client c1) {
            // TraceLog(LOG_INFO, "4. NEW PLAYER");
            sendNewGameSoon(c1, gr.gameManager.game_start_time);
            // handleUpdateGameState(c1);
            return true;
          });
        }
        gr.thread = std::thread(&Server::new_game, this, gr.room);
      }
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
    std::cout << "Clients for room " << gr.room.room_id
              << json(gr.clients).dump() << std::endl;
    for (auto c : gr.clients) {
      // if (c == except_client_id) {
      //   continue;
      // }

      TraceLog(LOG_INFO, "setting up updating rooms for %lu", c);
      todos.at(c).push([&](Client c1) {
        TraceLog(LOG_INFO, "updating rooms for %lu", c1.client_id);
        bool status = sendUpdateRoomState(c1);
        TraceLog(LOG_INFO, "updating rooms for %lu status=%d", c1.client_id,
                 status);
        return status;
      });
    }
  } catch (const std::out_of_range &ex) {
  }
  // try {
  //   auto &gr = games.at(client.room_id);
  //   std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
  //   for (auto c : gr.clients) {
  //     if (c == client.client_id) {
  //       continue;
  //     }
  //
  //     TraceLog(LOG_INFO, "queuing room update for client_id=%lu", c);
  //     todos.at(c).push([&](Client c1) {
  //       bool status = sendUpdateRoomState(c1);
  //       if (status) {
  //         TraceLog(LOG_INFO, "send room updated for client_id=%lu",
  //                  c1.client_id);
  //         TraceLog(LOG_INFO, "send room client_id= %lu, %s", c1.client_id,
  //                  json(gr.room).dump().c_str());
  //       }
  //       return status;
  //     });
  //   }
  // } catch (const std::out_of_range &ex) {
  // }
}

void Server::handlePlayerMovement(Client &client) {
  Vector2 position, velocity;
  float rotation;
  json movement;
  bool status = read_json(client.fd_main, movement, -1);
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
  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    auto &player = gr.gameManager.players.at(client.player_id);
    player.position = position;
    player.rotation = rotation;
    player.velocity = velocity;

    TraceLog(LOG_INFO, "client id movement %lu", client.client_id);
    for (auto c : gr.clients) {
      if (c == client.client_id)
        continue;

      TraceLog(LOG_INFO, "setting up update players for %lu", c);
      todos.at(c).push([&](Client c1) {
        TraceLog(LOG_INFO, "updating players for %lu", c1.client_id);
        serverSetEvent(c1, NetworkEvents::PlayerMovement);
        write_uint32(c1.fd_main, client.player_id);
        TraceLog(LOG_INFO, "updating player %lu", client.player_id);

        auto &player1 = gr.gameManager.players.at(client.player_id);

        json movement1 = {{"position", player1.position},
                          {"velocity", player1.velocity},
                          {"rotation", player1.rotation},
                          {"active", player1.active}};
        write_json(c1.fd_main, movement1);
        TraceLog(LOG_INFO, "updating movement for %lu, %s", c1.client_id,
                 movement1.dump().c_str());
        return true;
      });
    }
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

  if (!status) {
    TraceLog(LOG_INFO, "Player id=%lu cannot shoot bullet", client.player_id);
    return;
  }

  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    for (auto c : gr.clients) {
      todos.at(c).push([&](Client c1) {
        serverSetEvent(c1, NetworkEvents::ShootBullets);

        bool status = write_uint32(c1.fd_main, client.player_id);
        if (!status) {
          TraceLog(LOG_WARNING,
                   "Couldn't send info about bullet shot by player_id=%lu to "
                   "client_id=%ld,fd=%d",
                   client.player_id, client.client_id, client.fd_main);
          client_error(client);
        }
        return status;
      });
    }
  } catch (const std::out_of_range &ex) {
  }
}

void Server::new_game(const Room r) {
  while ((games.at(r.room_id).gameManager.game_start_time - time(0)) >
         0.1) { // TODO: thread sleep,
    // FIXME: restart if new player joins
  }
  TraceLog(LOG_INFO, "New round");
  {
    TraceLog(LOG_INFO, "1nr");
    {
      auto &gr = games.at(r.room_id);
      std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
      gr.room.status = GameStatus::GAME;
      // gr.gameManager.status = GameStatus::GAME;
    }
    TraceLog(LOG_INFO, "2nr");
    auto &gr = games.at(r.room_id);
    TraceLog(LOG_INFO, "3nr");
    for (auto c : gr.clients) {
      Client &cl = clients.at(c);
      gr.gameManager.players.at(cl.player_id).active = true;
    }
    for (auto c : gr.clients) {
      todos.at(c).push([&](Client c1) {
        serverSetEvent(c1, NetworkEvents::StartRound);
        handleUpdateGameState(c1);
        handleUpdatePlayers(c1);
        return true;
      });
      TraceLog(LOG_INFO, "6nr");
    }

    // game.UpdateStatus();
    // game.status = GameStatus::GAME;
    // TraceLog(LOG_DEBUG, "Game status: %d", gameManager.status);
    auto game_start_time = std::chrono::steady_clock::now();
    auto frame_start_time = std::chrono::steady_clock::now();
    duration<double> frametime = game_start_time - steady_clock::now();
    TraceLog(LOG_INFO, "game loop");
    while (games.at(r.room_id).room.status == GameStatus::GAME) {
      // TraceLog(LOG_INFO, "ROOM %lu is in game", r.room_id);
      {
        auto frame_start_time1 = std::chrono::steady_clock::now();
        frametime = frame_start_time1 - frame_start_time;
        frame_start_time = frame_start_time1;
        auto &gr = games.at(r.room_id);
        std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
        auto &game = gr.gameManager;

        // float frametime = GetFrameTime();

        // game.ManageCollisions();

        for (auto &player : game.players) {

          // player.rotation += Constants::PLAYER_ROTATION_SPEED * frametime;

          // if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
          //   Vector2 direction = {cosf(DEG2RAD * player.rotation),
          //                        sinf(DEG2RAD * player.rotation)};
          //   player.velocity = Vector2Add(
          //       player.velocity,
          //       Vector2Scale(direction, Constants::PLAYER_ACCELERATION *
          //       frametime));
          // }

          // Apply damping to velocity
          // player.velocity = Vector2Scale(
          //     player.velocity, (1 - Constants::PLAYER_DRAG / 1000.0f));

          // Update position
          // player.position = Vector2Add(
          //     player.position, Vector2Scale(player.velocity, frametime));

          // Keep player on the screen (wrap around)
          if (player.position.x < 0)
            player.position.x += Constants::screenWidth;
          if (player.position.x > Constants::screenWidth)
            player.position.x -= Constants::screenWidth;
          if (player.position.y < 0)
            player.position.y += Constants::screenHeight;
          if (player.position.y > Constants::screenHeight)
            player.position.y -= Constants::screenHeight;

          // Update player color depending on the active state
          player.player_color.a = player.active ? 255 : 25;
          // TraceLog(LOG_INFO, json(player).dump().c_str());
        }

        // FIXME: ENDED HERE IMPLEMENT BULLETS

        game.UpdateBullets(frametime);
        game.UpdateAsteroids(frametime);
        if (game.AsteroidSpawner()) { // FIXME: send only the asteroid that
                                      // updated
          for (auto c : gr.clients) {
            todos.at(c).push([&](Client c1) {
              TraceLog(LOG_INFO, "Updating asteroids for client_id=%lu",
                       c1.client_id);
              handleUpdateAsteroids(c1);
              return true;
            });
          }
        }
        // TraceLog(LOG_INFO, json(game.players).dump().c_str());
        // if (game._alive_players < 2) {
        //   gs = GameStatus::END_OF_ROUND;
        //   game.status = GameStatus::END_OF_ROUND;
        // }
      }
    }
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
          gr.gameManager = GameManager{gr.room.room_id, gr.room.players};
          TraceLog(LOG_INFO, "1. NEW PLAYER");
        }
        TraceLog(LOG_INFO, json(gr.room).dump().c_str());
        TraceLog(LOG_INFO, "Client player id=%lu", client.player_id);
      }
    }
    client.room_id = read_room_id;
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  TraceLog(LOG_INFO, "Sending JoinRoom to id=%lu", client.player_id);
  serverSetEvent(client, NetworkEvents::JoinRoom);

  TraceLog(LOG_INFO, "Sending read room join to id=%lu", client.player_id);
  status = write_uint32(client.fd_main, (uint32_t)status * read_room_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send joined room id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  TraceLog(LOG_INFO, "Sending playerid to id=%lu", client.player_id);
  status = write_uint32(client.fd_main, client.player_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send player id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    for (auto c : gr.clients) {
      TraceLog(LOG_INFO, "setting up updating rooms for %lu", c);
      todos.at(c).push([&](Client c1) {
        TraceLog(LOG_INFO, "updating rooms for %lu", c1.client_id);
        bool status = sendUpdateRoomState(c1);
        TraceLog(LOG_INFO, "updating rooms for %lu status=%d", c1.client_id,
                 status);
        return status;
      });
    }
  } catch (const std::out_of_range &ex) {
  }
}

void Server::handleLeaveRoom(Client &client) {
  bool _ = false;
  TraceLog(LOG_INFO, "1. Leave room %lu", client.client_id);
  std::vector<uint32_t> client_ids;
  try {
    auto &gr = games.at(client.room_id);
    TraceLog(LOG_INFO, "2. Leave room %lu", client.client_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    auto i = gr.clients.begin();
    for (; i != gr.clients.end(); i++) {
      if (*i == client.client_id) {
        client_ids = gr.clients;
        // Found, remove client from GameRoom
        gr.clients.erase(i, i);
        _ = true;
        try {
          TraceLog(LOG_INFO, "3. Leave room %lu", client.client_id);
          gr.room.players.at(client.player_id).state = PlayerInfo::NONE;
          TraceLog(LOG_INFO, "4. Leave room %lu", client.client_id);
          gr.gameManager.players.at(client.player_id).active = false;
          TraceLog(LOG_INFO, "5. Leave room %lu", client.client_id);
          if (gr.clients.size() < 2) {
            gr.room.status = GameStatus::LOBBY;
            // gr.gameManager.status = GameStatus::LOBBY;
            if (gr.thread.joinable()) {

              gr.thread.join();
            }
          }
          TraceLog(LOG_INFO, "6. Leave room %lu", client.client_id);
          // TraceLog(LOG_INFO, "2. Leave room %lu", client.client_id);
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
  // TraceLog(LOG_INFO, "3. Leave room %lu", client.client_id);
  try {
    auto except_client_id = client.client_id;
    // TraceLog(LOG_INFO, "2. Leave room %lu", client.client_id);
    for (auto c : client_ids) {
      if (c == except_client_id) {
        continue;
      }
      // TraceLog(LOG_INFO, "4. Leave room %lu", client.client_id);

      todos.at(c).push([&](Client c1) {
        bool status = sendUpdateRoomState(c1);
        return status;
      });
    }
  } catch (const std::out_of_range &ex) {
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
    TraceLog(LOG_INFO, room_json.dump().c_str());
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
  if (client.epd > 0) {
    close(client.epd);
  }
  if (client.fd_main > 2) {
    // No info for client
    {
      try {
        {
          std::lock_guard<std::mutex> lc(clients_mutex);
          todos.erase(client.client_id);
          clients.erase(client.client_id);
        }
        {
          auto &g = games.at(client.room_id);
          std::lock_guard<std::mutex> lr(g.gameRoomMutex);

          auto i =
              std::remove(g.clients.begin(), g.clients.end(), client.client_id);
          g.clients.erase(i, g.clients.end());
          TraceLog(LOG_INFO, "Disconnected lient_id=%ld,fd=%d",
                   client.client_id, client.fd_main);
        }
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
  for (PlayerIdState &player : gr.room.players) {
    if (player.state == PlayerInfo::NONE) {
      player.state = PlayerInfo::NOT_READY;
      TraceLog(LOG_INFO, "Next player id: %lu", player_id);
      return player_id;
    }
    player_id++;
  }

  return -1;
}
