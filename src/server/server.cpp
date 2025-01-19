#include "server.hpp"
#include <chrono>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
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

#include "asteroid.hpp"
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
    std::thread(&Server::handle_connection, this, client_main_fd).detach();
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
  if (!status) {
    TraceLog(LOG_WARNING, "%s not received from client_id=%ld,fd=%d",
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

void Server::handle_connection(int client_fd) {

  uint32_t client_id = handleGetClientId(client_fd);
  if (client_id == 0) {
    return;
  }

  Client *client_ptr = nullptr;
  try {
    std::lock_guard<std::mutex> lg(clients_mutex);
    client_ptr = &clients.at(client_id);
  } catch (const std::out_of_range &ex) {
    return;
  }
  Client &client = *client_ptr;

  const size_t MAX_EVENTS = 3;
  epoll_event ee, events[MAX_EVENTS];
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    TraceLog(LOG_ERROR, "Couldn't epoll_create for fd=%d", client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
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
  ee.events = EPOLLIN;
  ee.data.fd = client.todo_fd;
  if (epoll_ctl(client.epd, EPOLL_CTL_ADD, client.todo_fd, &ee) == -1) {
    close(client.epd);
    TraceLog(
        LOG_ERROR,
        "Couldn't instantiate EPOLLIN for eventfd=%d for client_id=%ld,fd=%d",
        client.todo_fd, client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }

  TraceLog(LOG_DEBUG, "Added queue to epoll for client_id=%ld,fd=%d",
           client.client_id, client.fd_main);

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
          TraceLog(LOG_DEBUG,
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

        // if game/round end
        // FIXME: send END ROUND
        // FIXME: send START ROUND
      }
    }
  }
}

uint32_t Server::handleGetClientId(int client_fd) {
  if (!expectEvent(client_fd, NetworkEvents::GetClientId)) {
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return 0;
  }

  uint32_t client_id;
  bool status = read_uint32(client_fd, client_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't read new client_id from fd=%d", client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return 0;
  }
  if (client_id == 0) {
    client_id = _next_client_id++;
  } else {
    TraceLog(LOG_WARNING, "Non zero client_id received from fd=%d", client_fd);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return 0;
  }
  {
    std::lock_guard<std::mutex> lg(clients_mutex);
    TraceLog(LOG_DEBUG, "Creating client entry in clients for %lu", client_id);
    Client &c = clients[client_id];
    c.client_id = client_id;
    c.fd_main = client_fd;

    // std::vector<std::vector<uint32_t>> ccc;
    // for (auto &m : clients) {
    //   ccc.push_back(std::vector<uint32_t>{m.first, m.second.player_id});
    // }
    // TraceLog(LOG_INFO, "Current Clients: %s", json(ccc).dump().c_str());
    // remove debug

    TraceLog(LOG_DEBUG, "Creating todo queue for client_id=%ld,fd=%d",
             client_id, client_fd);
    try {
      auto &todo = todos[client_id]; // Create todo queue and add to epoll
      c.todo_fd = todo.get_event_fd();
    } catch (std::runtime_error &r) {
      TraceLog(LOG_ERROR, "Unable to create todo queue for client_id=%ld,fd=%d",
               client_id, client_fd);
      clients.erase(client_id);
      shutdown(client_fd, SHUT_RDWR);
      close(client_fd);
      return 0;
    }
  }

  status = write_uint32(client_fd, NetworkEvents::GetClientId);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write GetClientId NetworkEvent to client_id=%ld,fd=%d",
             client_id, client_fd);
    std::lock_guard<std::mutex> lg(clients_mutex);
    todos.erase(client_id);
    clients.erase(client_id);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return 0;
  }

  status = write_uint32(client_fd, client_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't write new client id to client_id=%ld,fd=%d",
             client_id, client_fd);
    std::lock_guard<std::mutex> lg(clients_mutex);
    todos.erase(client_id);
    clients.erase(client_id);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return 0;
  }

  return client_id;
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
      TraceLog(LOG_INFO, "VOTE READY %d", client.client_id);
      // client.player_id = get_next_available_player_id(gr);
      gr.room.players.at(client.player_id).state = PlayerInfo::READY;
      gr.gameManager.players[client.player_id].active =
          true; // TODO: check leave room
      player_short_infos_json = gr.room.players;
      std::cout << json(gr.room).dump() << std::endl;
      if (get_X_players(gr.room.players, READY) >= 2 &&
          gr.room.status != GameStatus::GAME) {
        std::thread(&Server::new_game, this, gr.room).detach();
        TraceLog(LOG_INFO, "game Thread started %d", client.client_id);
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
    // std::cout << "Clients for room " << gr.room.room_id
    //           << json(gr.clients).dump() << std::endl;
    for (auto c : gr.clients) {
      // if (c == except_client_id) {
      //   continue;
      // }

      TraceLog(LOG_DEBUG, "setting up updating rooms for %lu", c);
      todos.at(c).push([&](Client c1) {
        bool status = sendUpdateRoomState(c1);
        TraceLog(LOG_DEBUG, "updating rooms for %lu status=%d", c1.client_id,
                 status);
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

    TraceLog(LOG_DEBUG, "client id movement %lu", client.client_id);
    for (auto c : gr.clients) {
      if (c == client.client_id)
        continue;

      todos.at(c).push([&](Client c1) {
        TraceLog(LOG_DEBUG, "updating players for %lu", c1.client_id);
        serverSetEvent(c1, NetworkEvents::PlayerMovement);
        write_uint32(c1.fd_main, client.player_id);

        auto &player1 = gr.gameManager.players.at(client.player_id);

        json movement1 = {{"position", player1.position},
                          {"velocity", player1.velocity},
                          {"rotation", player1.rotation},
                          {"active", player1.active}};
        write_json(c1.fd_main, movement1);
        TraceLog(LOG_DEBUG, "updating movement for %lu, %s", c1.client_id,
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
    const Player &p = gr.gameManager.players.at(client.player_id);
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
        // std::cerr << c1.fd_main << " " << client.player_id << std::endl;
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

void Server::restart_timer(GameRoom &gr, std::vector<uint32_t> &last_clients) {
  gr.gameManager._spawnerTime = steady_clock::now();
  gr.gameManager.game_start_time =
      system_clock::now() + Constants::LOBBY_READY_TIME;
  last_clients = gr.clients;
  for (auto c : gr.clients) {
    auto time = system_clock::to_time_t(gr.gameManager.game_start_time);
    todos.at(c).push([=](Client c1) {
      sendUpdateRoomState(c1);
      sendNewGameSoon(c1, time);
      TraceLog(LOG_INFO, "Sent new game soon");
      // handleUpdateGameState(c1);
    });
  }
}

void Server::new_game(const Room r) {

  std::vector<uint32_t> destroyed_asteroids;
  destroyed_asteroids.resize(Constants::ASTEROIDS_MAX);
  std::vector<uint32_t> spawned_asteroids;
  spawned_asteroids.resize(Constants::ASTEROIDS_MAX);
  std::vector<uint32_t> destroyed_players_ids;
  destroyed_players_ids.reserve(Constants::PLAYERS_MAX);
  std::vector<uint32_t> destroyed_bullets_ids;
  destroyed_bullets_ids.reserve(Constants::BULLETS_PER_PLAYER *
                                Constants::PLAYERS_MAX);

  auto &gr = games.at(r.room_id);
  TraceLog(LOG_DEBUG, "New round for room %lu", r.room_id);
  auto last_clients = gr.clients;
  restart_timer(gr, last_clients);
  // for (uint32_t round_counter = 0; round_counter <
  // Constants::ROUNDS_PER_GAME;
  //      round_counter++) {
  do {
    std::this_thread::sleep_until(gr.gameManager.game_start_time - 0.1s);
    if (gr.clients.size() != last_clients.size()) {
      restart_timer(gr, last_clients);
      continue;
    }
    for (size_t i = 0; i < last_clients.size(); i++) {
      if (last_clients.at(i) != gr.clients.at(i)) {
        restart_timer(gr, last_clients);
        break;
      }
    }
  } while ((gr.gameManager.game_start_time - system_clock::now()) > 100ms);
  gr.room.status = GameStatus::GAME;
  gr.gameManager.NewGame(gr.room.players);
  for (auto c : gr.clients) {
    todos.at(c).push([=](Client c1) {
      TraceLog(LOG_INFO, "START ROUND SENT TO %d", c1.client_id);
      serverSetEvent(c1, NetworkEvents::StartRound);
      handleUpdatePlayers(c1);
      // handleUpdateAsteroids(c1); // FIXME:
    });
  }
  auto game_start_time = std::chrono::steady_clock::now();
  auto frame_start_time = std::chrono::steady_clock::now();
  duration<double> frametime = game_start_time - steady_clock::now();

  while (games.at(r.room_id).room.status == GameStatus::GAME) {
    {
      auto frame_start_time1 = std::chrono::steady_clock::now();
      frametime = frame_start_time1 - frame_start_time;
      // std::cerr << "frametime: " << frametime.count() << std::endl;
      frame_start_time = frame_start_time1;
      auto &gr = games.at(r.room_id);
      std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
      auto &game = gr.gameManager;

      destroyed_asteroids.clear();
      spawned_asteroids.clear();
      destroyed_players_ids.clear();
      destroyed_bullets_ids.clear();

      game.ManageCollisions(destroyed_asteroids, spawned_asteroids,
                            destroyed_players_ids, destroyed_bullets_ids);

      for (auto &player : game.players) {

        player.rotation += Constants::PLAYER_ROTATION_SPEED * frametime.count();

        // Apply damping to velocity
        player.velocity = Vector2Scale(player.velocity,
                                       (1 - Constants::PLAYER_DRAG / 1000.0f));

        // Update position
        player.position = Vector2Add(
            player.position, Vector2Scale(player.velocity, frametime.count()));

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
      }
      game.UpdateBullets(frametime);
      game.UpdateAsteroids(frametime);
      game.AsteroidSpawner(spawned_asteroids);

      for (auto b : destroyed_bullets_ids) {
        for (auto c : gr.clients) {
          todos.at(c).push([=](Client c1) { // TODO: send timestamp
            TraceLog(LOG_DEBUG, "Updating bullets for client_id=%lu",
                     c1.client_id);
            handleBulletDestroyed(c1, b);
          });
        }
      }

      for (auto id : spawned_asteroids) {
        Asteroid &a = game.asteroids.at(id);
        for (auto c : gr.clients) { // TODO: send timestamp
          todos.at(c).push([=](Client c1) {
            TraceLog(LOG_DEBUG, "Updating asteroids for client_id=%lu",
                     c1.client_id);
            handleSpawnAsteroid(c1, a, id);
          });
        }
      }

      for (auto id : destroyed_asteroids) {
        for (auto c : gr.clients) { // TODO: send timestamp
          todos.at(c).push([=](Client c1) {
            TraceLog(LOG_DEBUG, "Updating asteroids for client_id=%lu",
                     c1.client_id);
            handleAsteroidDestroyed(c1, id);
          });
        }
      }

      for (auto p : destroyed_players_ids) {
        for (auto c : gr.clients) {
          todos.at(c).push([=](Client c1) { // TODO: send timestamp
            TraceLog(LOG_DEBUG, "Updating players for client_id=%lu",
                     c1.client_id);
            handlePlayerDestroyed(c1, p);
          });
        }
      }
      // TraceLog(LOG_INFO, json(game.players).dump().c_str());

      int active_players = 0;
      uint32_t winner = UINT32_MAX;
      for (auto &p : gr.gameManager.players) {
        if (p.active) {
          active_players += 1;
          winner = p.player_id;
        }
      }
      if (active_players < 2) {
        gr.room.status = GameStatus::LOBBY;
        gr.gameManager.game_start_time =
            system_clock::now() + Constants::NEW_ROUND_WAIT_TIME;
        auto time = system_clock::to_time_t(gr.gameManager.game_start_time);
        for (auto c : gr.clients) {
          todos.at(c).push([=](Client c1) {
            TraceLog(LOG_INFO, "END OF ROUND SENT TO %d", c1.client_id);
            // handleUpdateGameState(c1);
            serverSetEvent(c1, NetworkEvents::EndRound);
            write_uint32(c1.fd_main, winner);
            write_uint32(c1.fd_main, time);
          });
        }
      }
    }
  }
  // }
  gr.room.status = GameStatus::LOBBY;
  for (auto &p : gr.room.players) {
    if (p.state == PlayerInfo::READY)
      p.state = PlayerInfo::NOT_READY;
  }
  for (auto c : gr.clients) {
    gr.gameManager.game_start_time =
        system_clock::now() - Constants::NEW_ROUND_WAIT_TIME; // invalid time
    auto when = system_clock::to_time_t(gr.gameManager.game_start_time);
    todos.at(c).push([=](Client c1) {
      TraceLog(LOG_INFO, "LOBBY SENT TO %d", c1.client_id);
      serverSetEvent(c1, NetworkEvents::ReturnToLobby);
      write_uint32(c1.fd_main, when);
      sendUpdateRoomState(c1);
    });
  }
  TraceLog(LOG_INFO, "game Thread ended room");
  std::cout << json(gr.room).dump() << std::endl;
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
  TraceLog(LOG_DEBUG, "Received room id=%lu to join from client_id=%lu,fd=%d",
           read_room_id, client.client_id, client.fd_main);

  try {
    {
      auto &gr = games.at(read_room_id);
      std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
      status = gr.room.status == GameStatus::LOBBY;
      if (status) {
        auto player_id = get_next_available_player_id(gr);
        // std::cerr << player_id << std::endl;
        status = player_id != UINT32_MAX;
        client.player_id = player_id;
        if (status) {
          gr.room.players.at(player_id).state = PlayerInfo::NOT_READY;
          gr.clients.push_back(client.client_id);
          gr.gameManager = GameManager{gr.room.room_id, gr.room.players};
        }
        TraceLog(LOG_DEBUG, "Client player id=%lu", client.player_id);
      }
    }
    client.room_id = read_room_id;
  } catch (const std::out_of_range &ex) {
    status = false;
  }

  TraceLog(LOG_DEBUG, "Sending JoinRoom to id=%lu", client.player_id);
  serverSetEvent(client, NetworkEvents::JoinRoom);

  TraceLog(LOG_DEBUG, "Sending read room join to id=%lu", client.player_id);
  status = write_uint32(client.fd_main, (uint32_t)status * read_room_id);
  if (!status) {
    TraceLog(LOG_WARNING, "Couldn't send joined room id to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    client_error(client);
  }

  TraceLog(LOG_DEBUG, "Sending playerid to id=%lu", client.player_id);
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
      todos.at(c).push([&](Client c1) {
        TraceLog(LOG_DEBUG, "updating rooms for %lu", c1.client_id);
        sendUpdateRoomState(c1);
      });
    }
  } catch (const std::out_of_range &ex) {
  }
}

void Server::handleLeaveRoom(Client &client) {
  bool _ = false;
  std::vector<uint32_t> client_ids;
  try {
    auto &gr = games.at(client.room_id);
    std::lock_guard<std::mutex> lg(gr.gameRoomMutex);
    auto i = gr.clients.begin();
    for (; i != gr.clients.end(); i++) {
      if (*i == client.client_id) {
        client_ids = gr.clients;
        // Found, remove client from GameRoom
        gr.clients.erase(i);
        _ = true;
        try {
          gr.room.players.at(client.player_id).state = PlayerInfo::NONE;
          gr.gameManager.players.at(client.player_id).active = false;
          if (gr.clients.size() < 2) {
            gr.room.status = GameStatus::LOBBY;
          }
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
  for (auto c : client_ids) {
    if (c == client.client_id)
      continue;
    try {
      todos.at(c).push([=](Client c1) {
        serverSetEvent(c1, NetworkEvents::LeaveRoom);
        bool status = write_uint32(c1.fd_main, client.player_id);
        if (!status) {
          TraceLog(LOG_WARNING,
                   "Couldn't send leaving room confirmation to "
                   "client_id=%ld,fd=%d",
                   client.room_id, client.client_id, client.fd_main);
          client_error(c1);
        }
        sendUpdateRoomState(c1);
      });
    } catch (const std::out_of_range &ex) {
    }
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
    TraceLog(LOG_DEBUG, room_json.dump().c_str());
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
    serverSetEvent(client, NetworkEvents::UpdateAsteroids);
    json j = games.at(client.room_id).gameManager.asteroids;
    bool status = write_json(client.fd_main, j);
    if (!status) {
      TraceLog(LOG_WARNING, "Couldn't send asteroids to client_id=%ld,fd=%d",
               client.room_id, client.client_id, client.fd_main);
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
// void Server::handleUpdateAsteroids(Client &client,
//                                    const std::vector<uint32_t> &asteroid_ids,
//                                    const std::vector<Asteroid> &asteroids) {
//   try {
//     bool status;
//     json asteroids_json = asteroids;
//     json asteroid_ids_json = asteroid_ids;
//     serverSetEvent(client, NetworkEvents::UpdateAsteroids);
//
//     status = write_json(client.fd_main, asteroid_ids_json);
//     if (!status) {
//       TraceLog(LOG_WARNING,
//                "Couldn't send json of asteroid ids to client_id=%ld,fd=%d",
//                client.client_id, client.fd_main);
//       client_error(client);
//       return;
//     }
//     status = write_json(client.fd_main, asteroids_json);
//     if (!status) {
//       TraceLog(LOG_WARNING,
//                "Couldn't send json of asteroids to client_id=%ld,fd=%d",
//                client.client_id, client.fd_main);
//       client_error(client);
//       return;
//     }
//   } catch (const std::out_of_range &ex) {
//     TraceLog(LOG_WARNING, "Invalid room id %lu from client_id=%ld,fd=%d",
//              client.room_id, client.client_id, client.fd_main);
//     client_error(client);
//     return;
//   }
// }

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

void Server::invalid_network_event(Client &client, uint32_t event) {
  TraceLog(LOG_WARNING, "%s received from client_id=%ld,fd=%d",
           network_event_to_string(event).c_str(), client.client_id,
           client.fd_main);
  disconnect_client(client);
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
    invalid_network_event(client, event);
    break;
  case NetworkEvents::StartRound:
    // Never read by server
    invalid_network_event(client, event);
    break;
  case NetworkEvents::GetClientId:
    // Never read by server after establishing connection
    invalid_network_event(client, event);
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
  case NetworkEvents::NewGameSoon:
    // Not received by server
    invalid_network_event(client, event);
    break;
  case NetworkEvents::PlayerDestroyed:
    // Not received by server
    invalid_network_event(client, event);
    break;
  case NetworkEvents::BulletDestroyed:
    // Not received by server
    invalid_network_event(client, event);
    break;
  default:
    invalid_network_event(client, event);
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
          TraceLog(LOG_DEBUG, "Disconnected lient_id=%ld,fd=%d",
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

// FIXME: good disconnect
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
      TraceLog(LOG_DEBUG, "Next player id: %lu", player_id);
      return player_id;
    }
    player_id++;
  }

  return -1;
}

void Server::handleBulletDestroyed(Client &client, uint32_t bullet_id) {
  serverSetEvent(client, NetworkEvents::BulletDestroyed);
  bool status = write_uint32(client.fd_main, bullet_id);
  if (!status) {
    TraceLog(
        LOG_WARNING,
        "Couldn't write NetworkEvent BulletDestroyed to client_id=%ld,fd=%d",
        client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
  TraceLog(LOG_DEBUG,
           "sent NetworkEvent BulletDestroyed to client_id=%ld,fd=%d",
           client.client_id, client.fd_main);
}

void Server::handlePlayerDestroyed(Client &client, uint32_t player_id) {
  serverSetEvent(client, NetworkEvents::PlayerDestroyed);
  bool status = write_uint32(client.fd_main, player_id);
  if (!status) {
    TraceLog(
        LOG_WARNING,
        "Couldn't write NetworkEvent PlayerDestroyed to client_id=%ld,fd=%d",
        client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
  TraceLog(LOG_DEBUG,
           "sent NetworkEvent PlayerDestroyed to client_id=%ld,fd=%d",
           client.client_id, client.fd_main);
}

void Server::handleSpawnAsteroid(Client &client, Asteroid a, uint32_t id) {
  serverSetEvent(client, NetworkEvents::SpawnAsteroid);
  bool status = write_uint32(client.fd_main, id);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write NetworkEvent SpawnAsteroid to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
  json j = a;
  status = write_json(client.fd_main, j);
  if (!status) {
    TraceLog(LOG_WARNING,
             "Couldn't write spawned asteroid to client_id=%ld,fd=%d",
             client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
}
void Server::handleAsteroidDestroyed(Client &client, uint32_t asteroid_id) {
  serverSetEvent(client, NetworkEvents::AsteroidDestroyed);
  bool status = write_uint32(client.fd_main, asteroid_id);
  if (!status) {
    TraceLog(
        LOG_WARNING,
        "Couldn't write NetworkEvent AsteroidDestroyed to client_id=%ld,fd=%d",
        client.client_id, client.fd_main);
    disconnect_client(client);
    return;
  }
}
