#include "networking.hpp"
#include <iostream>

#include <chrono>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

#include "asteroid.hpp"
#include "bullet.hpp"
#include "constants.hpp"
#include "gameManager.hpp"
#include "gameStatus.hpp"
#include "jsonutils.hpp"
#include "player.hpp"
#include "raylib.h"
#include "room.hpp"
#include "vec2json.hpp"

using json = nlohmann::json;

ClientNetworkManager::ClientNetworkManager(
    const char *host, const char *port,
    std::array<GameManager, 2> &gameManagersPair,
    std::atomic_uint8_t &game_manager_draw_idx,
    std::array<std::map<uint32_t, Room>, 2> &roomsPair,
    std::atomic_uint8_t &rooms_draw_idx, std::array<Room, 2> &joinedRoomPair,
    std::atomic_uint8_t &joined_room_draw_idx, std::atomic_uint32_t &player_id)
    : gameManagersPair(gameManagersPair),
      game_manager_draw_idx(game_manager_draw_idx), roomsMapPair(roomsPair),
      rooms_draw_idx(rooms_draw_idx), joinedRoomPair(joinedRoomPair),
      joined_room_draw_idx(joined_room_draw_idx), player_id(player_id),
      connected_to_host(host), connected_over_port(port) {
  mainfd = connect_to(host, port);
  if (mainfd < 0) {
    TraceLog(LOG_ERROR, "GAME: Could not connect");
    shutdown(mainfd, SHUT_RDWR);
    close(mainfd);
    return;
  }

  if (!get_new_client_id(client_id)) {
    TraceLog(LOG_ERROR, "GAME: Could not get a new client id");
  }
  // set_socket_timeout(5);
  main_thread =
      std::thread(&ClientNetworkManager::perform_network_actions, this);
}

ClientNetworkManager::~ClientNetworkManager() {
  bool expected = _stop.load();
  while (!_stop.compare_exchange_strong(expected, !expected)) {
  }
  if (main_thread.joinable()) {
    main_thread.join();
  }
  shutdown(mainfd, SHUT_RDWR);
  close(mainfd);
}

void ClientNetworkManager::flip_game_manager() {
  // std::cout << " flip gm: ";
  // std::cout << static_cast<int>(game_manager_draw_idx.load()) << " ";
  uint8_t expected = game_manager_draw_idx.load();
  while (!game_manager_draw_idx.compare_exchange_strong(expected, !expected)) {
  }
  // std::cout << static_cast<int>(game_manager_draw_idx.load()) << std::endl;
}

void ClientNetworkManager::flip_rooms() {
  // std::cout << " flip rooms: ";
  // std::cout << static_cast<int>(rooms_draw_idx.load()) << " ";
  uint8_t expected = rooms_draw_idx.load();
  while (!rooms_draw_idx.compare_exchange_strong(expected, !expected)) {
  }
  // std::cout << static_cast<int>(rooms_draw_idx.load()) << std::endl;
}

void ClientNetworkManager::flip_joined_room() {
  // std::cout << " flip joined room: ";
  // std::cout << static_cast<int>(joined_room_draw_idx.load()) << " ";
  uint8_t expected = joined_room_draw_idx.load();
  while (!joined_room_draw_idx.compare_exchange_strong(expected, !expected)) {
  }
  // std::cout << static_cast<int>(joined_room_draw_idx.load()) << std::endl;
}

void ClientNetworkManager::perform_network_actions() {
  const size_t MAX_EVENTS = 2;
  epoll_event ee, events[MAX_EVENTS];
  this->epollfd = epoll_create1(0);
  if (epollfd == -1) {
    TraceLog(LOG_ERROR, "NET: Couldn't instantiate epoll");
    shutdown(mainfd, SHUT_RDWR);
    close(mainfd);
    return;
  }

  ee.events = EPOLLIN;
  ee.data.fd = mainfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, mainfd, &ee) == -1) {
    TraceLog(LOG_ERROR, "NET: Couldn't add event to epoll");
    close(epollfd);
    shutdown(mainfd, SHUT_RDWR);
    close(mainfd);
    return;
  }

  ee.events = EPOLLIN;
  ee.data.fd = todo.get_event_fd();
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, todo.get_event_fd(), &ee) == -1) {
    TraceLog(LOG_ERROR, "NET: Couldn't add event to epoll");
    close(epollfd);
    shutdown(mainfd, SHUT_RDWR);
    close(mainfd);
    return;
  }

  while (!this->_stop) {
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS,
                          3 * Constants::CONNECTION_TIMEOUT_MILISECONDS);
    // Constants::CONNECTION_TIMEOUT_MILISECONDS);
    if (nfds == -1) { // EPOLL WAIT ERROR
      TraceLog(LOG_ERROR, "NET: Epoll wait error");
      close(epollfd);
      shutdown(mainfd, SHUT_RDWR);
      close(mainfd);
      return;
    } else if (nfds == 0) { // EPOLL WAIT TIMEOUT
      // RECONNECT
      shutdown(mainfd, SHUT_RDWR);
      close(mainfd);
      // reconnect(connected_to_host, connected_over_port, client_id);
    }
    for (int n = 0; n < nfds; n++) {
      if (events[n].data.fd == todo.get_event_fd()) {
        todo.pop()();
      } else if (events[n].data.fd == mainfd) {
        uint32_t network_event;
        bool status = read_uint32(mainfd, network_event);
        if (status) {
          TraceLog(LOG_DEBUG, "NET: Received NetworkEvent %s",
                   network_event_to_string(network_event).c_str());
          handle_network_event(network_event);
        } else {
          TraceLog(LOG_WARNING, "NET: Couldn't read NetworkEvent");
          shutdown(mainfd, SHUT_RDWR);
          close(mainfd);
          break;
        }
      }
    }
  }

  close(epollfd);
}

bool ClientNetworkManager::readGameState() {
  json game_state_json;
  bool status = read_json(mainfd, game_state_json, -1);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of game state");
    return false;
  }

  try {
    TraceLog(LOG_DEBUG, "NET: received json of game state");
    gameManager() = game_state_json.template get<GameManager>();
    auto &draw_gm_mgr = gameManagersPair.at(game_manager_draw_idx);
    gameManager().players = draw_gm_mgr.players;
    gameManager().asteroids = draw_gm_mgr.asteroids;
    gameManager().bullets = draw_gm_mgr.bullets;
    flip_game_manager();
    gameManager() = gameManagersPair.at(game_manager_draw_idx);
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into GameManager");
    return false;
  }
}

void ClientNetworkManager::handle_network_event(uint32_t event) {
  switch ((NetworkEvents)event) {
  case NetworkEvents::NoEvent:
    TraceLog(LOG_WARNING, "NET: %s received",
             network_event_to_string(event).c_str());
    break;
  case NetworkEvents::Disconnect:
    TraceLog(LOG_WARNING, "NET: %s received",
             network_event_to_string(event).c_str());
    break;
  case NetworkEvents::CheckConnection: {
    TraceLog(LOG_INFO, "NET:sending  %s",
             network_event_to_string(event).c_str());
    if (!write_uint32(mainfd, NetworkEvents::CheckConnection)) {
      TraceLog(LOG_WARNING, "NET: Cannot send %s",
               network_event_to_string(event).c_str());
      return;
    }
    break;
  }
  case NetworkEvents::EndRound: {
    uint32_t winner_player_id;
    bool status = read_uint32(mainfd, winner_player_id);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't receive winner's player id");
    }

    uint32_t val;
    status = read_uint32(mainfd, val);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Didn't receive round start time");
      return;
    }
    auto when =
        std::chrono::system_clock::time_point(std::chrono::seconds(val));
    gameManager().game_start_time = when;
    gameManager().winner_player_id = winner_player_id;
    flip_game_manager();
    gameManager().game_start_time = when;
    gameManager().winner_player_id = winner_player_id;
    joinedRoom().status = GameStatus::LOBBY;
    flip_joined_room();
    joinedRoom().status = GameStatus::LOBBY;
    std::cout << json(joinedRoom()).dump() << std::endl;
    std::cout << json(gameManager()).dump() << std::endl;
  } break;
  case NetworkEvents::StartRound:
    gameManager().NewGame(joinedRoom().players);
    flip_game_manager();
    gameManager().NewGame(joinedRoom().players);
    joinedRoom().status = GameStatus::GAME;
    flip_joined_room();
    joinedRoom().status = GameStatus::GAME;
    break;
  case NetworkEvents::NewGameSoon: {
    uint32_t val;
    bool status = read_uint32(mainfd, val);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Didn't receive game start time");
      return;
    }
    auto when =
        std::chrono::system_clock::time_point(std::chrono::seconds(val));
    gameManager().game_start_time = when;
    flip_game_manager();
    gameManager().game_start_time = when;
  } break;
  case NetworkEvents::ReturnToLobby: {
    uint32_t val;
    bool status = read_uint32(mainfd, val);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Didn't receive return to lobby time");
      return;
    }
    auto when =
        std::chrono::system_clock::time_point(std::chrono::seconds(val));
    joinedRoom().status = GameStatus::LOBBY;
    flip_joined_room();
    joinedRoom().status = GameStatus::LOBBY;
    gameManager().game_start_time = when;
    flip_game_manager();
    gameManager().game_start_time = when;
  } break;
  case NetworkEvents::GetClientId: {
    uint32_t new_client_id;
    bool status = read_uint32(mainfd, new_client_id);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Didn't receive full client id");
      return;
    }
    if (new_client_id == 0) {
      TraceLog(LOG_ERROR, "GAME: Received client id is 0");
      return;
    }
    client_id = new_client_id;
  } break;
  case NetworkEvents::VoteReady: {
    std::vector<PlayerIdState> players;
    json player_short_infos_json;
    bool status = read_json(mainfd, player_short_infos_json, -1);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't receive json of players");
      return;
    }
    try {
      players =
          player_short_infos_json.template get<std::vector<PlayerIdState>>();
    } catch (json::exception &ex) {
      TraceLog(LOG_ERROR,
               "JSON: Couldn't deserialize json into vector<Player>");
      return;
    }
    if (status) {
      joinedRoom().players = players;
      flip_joined_room();
      joinedRoom() = joinedRoomPair.at(joined_room_draw_idx);
    }
  } break;
  case NetworkEvents::PlayerMovement: {
    uint32_t updated_player_id;
    if (!read_uint32(mainfd, updated_player_id)) {
      TraceLog(LOG_ERROR, "NET: cannot read player id for updating movement",
               updated_player_id);
      return;
    }

    json movement;
    Vector2 position, velocity;
    float rotation;
    bool active;
    try {
      if (!read_json(mainfd, movement, -1)) {
        TraceLog(LOG_ERROR,
                 "NET: cannot read player movement json for player_id=%lu",
                 updated_player_id);
        return;
      }
      position = movement.at("position");
      velocity = movement.at("velocity");
      rotation = movement.at("rotation");
      active = movement.at("active");
      // std::cout << json(gameManager().players).dump() << std::endl;
      // TraceLog(LOG_INFO, "NET: player_id=%lu, player_data=%s",
      //          updated_player_id, movement.dump().c_str(),
      //          network_event_to_string(event).c_str());
    } catch (json::exception &ex) {
      TraceLog(LOG_ERROR, "JSON: Couldn't serialize movement into json");
      return;
    }

    try {
      gameManager() = gameManagersPair.at(game_manager_draw_idx);
      auto &player = gameManager().players.at(updated_player_id);
      player.position = position;
      player.velocity = velocity;
      player.rotation = rotation;
      player.active = active;
      flip_game_manager();
      gameManager() = gameManagersPair.at(game_manager_draw_idx);
    } catch (const std::out_of_range &ex) {
      TraceLog(LOG_ERROR, "JSON: updated player id ouf of range %lu",
               updated_player_id);
      return;
    }

  }

  break;
  case NetworkEvents::ShootBullets: {
    // TraceLog(LOG_WARNING, "NET: %s received",
    //          network_event_to_string(event).c_str());
    uint32_t value;
    bool status = read_uint32(mainfd, value);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't receive who shot the bullet");
      return;
    }
    try {
      gameManager() = gameManagersPair.at(game_manager_draw_idx);
      gameManager().AddBullet(gameManager().players.at(value));
      flip_game_manager();
      gameManager().bullets = gameManager().bullets;
    } catch (const std::out_of_range &ex) {
    }
  } break;
  case NetworkEvents::GetRoomList: {
    json rooms_json;
    bool status = read_json(mainfd, rooms_json, -1);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't receive json of rooms");
      return;
    }
    try {
      auto rs = rooms_json.template get<std::map<uint32_t, Room>>();
      rooms() = rs;
      flip_rooms();
      rooms() = rs;
      return;
    } catch (json::exception &ex) {
      TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into vector<Room>");
      return;
    }
  } break;
  case NetworkEvents::JoinRoom: {

    uint32_t joined_room_id;
    bool status = read_uint32(mainfd, joined_room_id);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't read joined room id");
      return;
    }
    uint32_t _player_id;

    if (joined_room_id > 0) {
      status = read_uint32(mainfd, _player_id);
      if (!status) {
        TraceLog(LOG_ERROR, "NET: Couldn't read player id");
        return;
      }

      TraceLog(LOG_INFO, "GAME: Joined room_id=%lu, player_id=%lu",
               joined_room_id, _player_id);
      uint32_t expected_player_id = -1;
      while (!this->player_id.compare_exchange_strong(expected_player_id,
                                                      _player_id)) {
      }
      expectEvent(mainfd, NetworkEvents::UpdateGameState);
      readGameState();
      joinedRoom().room_id = joined_room_id;
      flip_joined_room();
      joinedRoom().room_id = joined_room_id;
      TraceLog(LOG_INFO, "1.GAME: Joined room_id=%lu, player_id=%lu",
               joined_room_id, _player_id);
      return;
    } else {
      return;
    }
  }

  break;
  case NetworkEvents::LeaveRoom: {
    uint32_t player_id_that_left;
    bool status = read_uint32(mainfd, player_id_that_left);
    if (!status) {
      return;
    }
    gameManager().winner_player_id = UINT32_MAX;
    flip_game_manager();
    gameManager().winner_player_id = UINT32_MAX;

    if (player_id_that_left == player_id.load()) {

      player_id = -1;
      joinedRoom() = Room{0};
      flip_joined_room();
      joinedRoom() = Room{0};
      // auto expected_player_id = player_id.load();
      // if (expected_player_id == player_id_that_left) {
      //   while (!player_id.compare_exchange_strong(expected_player_id, -1)) {
      //   }
      // }
      TraceLog(LOG_DEBUG, "NET: Left room");
      return;
    }

    try {
      joinedRoom().players.at(player_id_that_left).state = PlayerInfo::NONE;
      flip_joined_room();
      joinedRoom().players.at(player_id_that_left).state = PlayerInfo::NONE;

    } catch (const std::out_of_range &ex) {
    }
  } break;
  case NetworkEvents::UpdateGameState: {
    readGameState();
  } break;
  case NetworkEvents::UpdateRoomState: {
    // TraceLog(LOG_WARNING, "NET: %s received",
    //          network_event_to_string(event).c_str());
    json room_state_json;
    bool status = read_json(mainfd, room_state_json, -1);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't receive json of room state");
      return;
    }
    try {
      TraceLog(LOG_DEBUG, "NET: received json of room state");
      auto room = room_state_json.template get<Room>();
      joinedRoom() = room;
      flip_joined_room();
      joinedRoom() = room;
    } catch (json::exception &ex) {
      TraceLog(LOG_ERROR,
               "JSON: Couldn't deserialize json into vector<Player>");
    } catch (const std::out_of_range &ex) {
    }
    std::cout << "update room state: " << json(joinedRoom()).dump()
              << std::endl;
  } break;
  case NetworkEvents::UpdatePlayers: {
    read_update_players();
  } break;
  case NetworkEvents::UpdateAsteroids: {
    handle_update_asteroids();
  } break;
  case NetworkEvents::UpdateBullets: {
    json bullets_json;
    bool status = read_json(mainfd, bullets_json, -1);
    if (!status) {
      TraceLog(LOG_ERROR, "NET: Couldn't receive json of players");
      return;
    }
    try {
      auto bullets = bullets_json.template get<std::vector<Bullet>>();
      gameManager() = gameManagersPair.at(game_manager_draw_idx);
      gameManager().bullets = bullets;
      flip_game_manager();
    } catch (json::exception &ex) {
      TraceLog(LOG_ERROR,
               "JSON: Couldn't deserialize json into vector<Bullet>");
      return;
    }
  } break;
  case NetworkEvents::PlayerDestroyed:
    handle_player_destroyed();
    break;
  case NetworkEvents::SpawnAsteroid:
    handle_spawn_asteroid();
    break;
  case NetworkEvents::AsteroidDestroyed:
    handle_asteroid_destroyed();
    break;
  case BulletDestroyed:
    handle_bullet_destroyed();
    break;
  default:
    TraceLog(LOG_WARNING, "Unknown NetworkEvent %ld received", event);
    break;
  }
}

bool ClientNetworkManager::get_new_client_id(uint32_t &client_id) {
  bool status;

  // Negotiate new client id
  status = write_uint32(mainfd, NetworkEvents::GetClientId);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Cannot send GetClientId NetworkEvent");
    return false;
  }

  // Get new id by sending id=0
  status = write_uint32(mainfd, client_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Cannot send client_id");
    return false;
  }

  status = expectEvent(mainfd, NetworkEvents::GetClientId);
  if (!status) {
    return false;
  }

  uint32_t new_client_id;
  status = read_uint32(mainfd, new_client_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Didn't receive full client id");
    return false;
  }
  if (new_client_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Received client id is 0");
    return false;
  }

  return true;
}

bool ClientNetworkManager::get_rooms() {
  bool status;
  status = write_uint32(mainfd, NetworkEvents::GetRoomList);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Cannot send GetRoomList NetworkEvent");
    return false;
  }
  return status;
}

int ClientNetworkManager::disconnect() {
  int status;
  status = setEvent(mainfd, NetworkEvents::Disconnect);
  if (!status) {
    return false;
  }

  status = shutdown(mainfd, SHUT_RDWR);
  if (status != 0) {
    TraceLog(LOG_ERROR, "Couldn't shutdown the connection properly");
    return status;
  }
  status = close(mainfd);
  if (status != 0) {
    TraceLog(LOG_ERROR, "Couldn't close the socket properly");
    return status;
  }

  return status;
}

// Sets a response timeout in seconds
// WARN: any read or write from socket may now return before sending full data
int ClientNetworkManager::set_socket_timeout(time_t seconds) {
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  int status;
  status =
      setsockopt(mainfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
  if (status != 0) {
    return status;
  }
  return setsockopt(mainfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv,
                    sizeof tv);
}

// Connect to a TankBusters server by specified host and port
int ClientNetworkManager::connect_to(const char *host, const char *port) {
  int status = -1;
  int fd = -1;
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  struct addrinfo *p_addr = nullptr;
  struct addrinfo *c_addr = nullptr;
  status = getaddrinfo(host, port, &hints, &p_addr);
  if (status != 0 || p_addr == nullptr) {
    TraceLog(LOG_ERROR, "Couldn't resolve hostname (%s) or service (%s)", host,
             port);
    return status;
  }

  for (c_addr = p_addr; c_addr != nullptr; c_addr = c_addr->ai_next) {
    fd = socket(p_addr->ai_family, p_addr->ai_socktype, p_addr->ai_protocol);
    if (fd == -1) {
      continue;
    }
    status = connect(fd, p_addr->ai_addr, p_addr->ai_addrlen);
    if (status != -1) {
      break;
    }

    close(fd);
  }

  freeaddrinfo(p_addr);
  if (c_addr == nullptr) {
    TraceLog(LOG_ERROR, "Couldn't connect to %s:%s", host, port);
    return -1;
  }

  return fd;
}

bool ClientNetworkManager::join_room(uint32_t join_room_id) {
  bool status;

  if (join_room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into join_room");
    return false;
  }
  status = setEvent(mainfd, NetworkEvents::JoinRoom);
  if (!status) {
    return false;
  }

  status = write_uint32(mainfd, join_room_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send Room ID");
    return false;
  }

  return status;
}

// Get info about currently joined room
bool ClientNetworkManager::fetch_room_state(Room &room) {
  return fetch_room_state(joinedRoom().room_id, room);
}

bool ClientNetworkManager::fetch_room_state() {
  if (joinedRoom().room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into join_room");
    return false;
  }

  bool status;
  status = setEvent(mainfd, NetworkEvents::UpdateRoomState);
  if (!status)
    return false;

  status = write_uint32(mainfd, joinedRoom().room_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
    return false;
  }

  status = expectEvent(mainfd, NetworkEvents::UpdateRoomState);
  if (!status)
    return false;

  json room_json = json(Room());
  status = read_json(mainfd, room_json, -1);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of Room");
    return false;
  }

  try {
    Room room;
    room = room_json.template get<Room>();
    joinedRoom() = room;
    flip_joined_room();
    joinedRoom() = room;

    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into Room");
    return false;
  }
}

// Get info about arbitrary room by room id
bool ClientNetworkManager::fetch_room_state(uint32_t fetch_room_id,
                                            Room &room) {
  if (fetch_room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into join_room");
    return false;
  }

  bool status;
  status = setEvent(mainfd, NetworkEvents::UpdateRoomState);
  if (!status)
    return false;

  status = write_uint32(mainfd, fetch_room_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
    return false;
  }

  status = expectEvent(mainfd, NetworkEvents::UpdateRoomState);
  if (!status)
    return false;

  json room_json = json(Room());
  status = read_json(mainfd, room_json, -1);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of Room");
    return false;
  }

  try {
    room = room_json.template get<Room>();
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into Room");
    return false;
  }
}

// Leave currently joined room
bool ClientNetworkManager::leave_room() {
  if (joinedRoom().room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Leaving Room without previously joining any");
    return false;
  }

  bool status;
  status = setEvent(mainfd, NetworkEvents::LeaveRoom);
  if (!status) {
    return false;
  }

  return status;
}

bool ClientNetworkManager::send_movement(Vector2 position, Vector2 velocity,
                                         float rotation) {
  TraceLog(LOG_DEBUG, "NET: starting to send player movement");
  json movement;
  try {
    movement = {
        {"position", position}, {"velocity", velocity}, {"rotation", rotation}};
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't serialize movement into json");
    return false;
  }

  bool status = setEvent(mainfd, NetworkEvents::PlayerMovement);
  if (!status) {
    return false;
  }

  status = write_json(mainfd, movement);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send json of movement");
    return false;
  }

  return true;
}

// Fetches GameManager state for currently joined room
// Players' state, asteroids and bullets are *NOT* fetched
bool ClientNetworkManager::fetch_game_state(GameManager &gameManager) {
  if (joinedRoom().room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch game state");
    return false;
  }

  bool status;
  status = setEvent(mainfd, NetworkEvents::UpdateGameState);
  if (!status)
    return false;

  status = expectEvent(mainfd, NetworkEvents::UpdateGameState);
  if (!status)
    return false;

  json game_json = json(GameManager());
  status = read_json(mainfd, game_json, sizeof(game_json));
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of GameManager");
    return false;
  }

  try {
    gameManager = game_json.template get<GameManager>();
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into GameManager");
    return false;
  }
}

bool ClientNetworkManager::fetch_players(std::vector<Player> &players) {
  if (joinedRoom().room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch players");
    return false;
  }

  bool status;
  status = setEvent(mainfd, NetworkEvents::UpdatePlayers);
  if (!status)
    return false;

  status = expectEvent(mainfd, NetworkEvents::UpdatePlayers);
  if (!status)
    return false;

  json players_json =
      json(std::vector<Player>(Constants::PLAYERS_MAX, Player()));
  status = read_json(mainfd, players_json, sizeof(players_json));
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Player>");
    return false;
  }

  try {
    players = players_json.template get<std::vector<Player>>();
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into vector<Player>");
    return false;
  }
}
bool ClientNetworkManager::handle_update_asteroids() {
  std::vector<Asteroid> asteroids;
  json asteroids_json;
  bool status = read_json(mainfd, asteroids_json, -1);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Asteroid>");
    return false;
  }

  try {
    asteroids = asteroids_json.template get<std::vector<Asteroid>>();
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR,
             "JSON: Couldn't deserialize json into vector<Asteroid>");
    return false;
  }

  try {
    gameManager().asteroids = asteroids;
  } catch (const std::out_of_range &ex) {
    return false;
  }

  flip_game_manager();
  gameManager().asteroids = asteroids;

  return true;
}

// bool ClientNetworkManager::update_asteroids() {
//   std::vector<Asteroid> asteroids;
//   std::vector<uint32_t> ids;
//
//   json asteroid_ids_json;
//   bool status = read_json(mainfd, asteroid_ids_json, -1);
//   if (!status) {
//     TraceLog(LOG_ERROR, "NET: Couldn't receive json of asteroid ids");
//     return false;
//   }
//
//   try {
//     ids = asteroid_ids_json.template get<std::vector<uint32_t>>();
//   } catch (json::exception &ex) {
//     TraceLog(LOG_ERROR,
//              "JSON: Couldn't deserialize json into vector<uint32_t>");
//     return false;
//   }
//
//   // constexpr auto MAX_ASTEROIDS_VECTOR_SIZE =
//   // sizeof(json(std::vector<Asteroid>(Constants::ASTEROIDS_MAX,
//   Asteroid())));
//
//   json asteroids_json;
//   status = read_json(mainfd, asteroids_json, -1);
//   if (!status) {
//     TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Asteroid>");
//     return false;
//   }
//
//   try {
//     asteroids = asteroids_json.template get<std::vector<Asteroid>>();
//   } catch (json::exception &ex) {
//     TraceLog(LOG_ERROR,
//              "JSON: Couldn't deserialize json into vector<Asteroid>");
//     return false;
//   }
//   // if (!fetch_asteroids(ids, asteroids)) {
//   //   return false;
//   // }
//
//   if (asteroids.size() != ids.size()) {
//     return false;
//   }
//
//   try {
//     for (uint32_t i = 0; i < ids.size(); i++) {
//       gameManager().asteroids.at(ids.at(i)) = asteroids.at(i);
//     }
//   } catch (const std::out_of_range &ex) {
//     return false;
//   }
//
//   flip_game_manager();
//   gameManager().asteroids =
//       gameManagersPair.at(game_manager_draw_idx).asteroids;
//
//   return true;
// }
// bool ClientNetworkManager::fetch_asteroids(std::vector<uint32_t>
// &asteroid_ids,
//                                            std::vector<Asteroid> &asteroids)
//                                            {
//   if (joinedRoom().room_id == 0) {
//     TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch asteroids");
//     return false;
//   }
//
//   bool status;
//   status = setEvent(mainfd, NetworkEvents::UpdateAsteroids);
//   if (!status)
//     return false;
//
//   status = expectEvent(mainfd, NetworkEvents::UpdateAsteroids);
//   if (!status)
//     return false;
//
//   json asteroid_ids_json;
//   status = read_json(mainfd, asteroid_ids_json, -1);
//   if (!status) {
//     TraceLog(LOG_ERROR, "NET: Couldn't receive json of asteroid ids");
//     return false;
//   }
//   try {
//     asteroid_ids = asteroid_ids_json.template get<std::vector<uint32_t>>();
//   } catch (json::exception &ex) {
//     TraceLog(LOG_ERROR,
//              "JSON: Couldn't deserialize json into vector<uint32_t>");
//     return false;
//   }
//
//   // constexpr auto MAX_ASTEROIDS_VECTOR_SIZE =
//   // sizeof(json(std::vector<Asteroid>(Constants::ASTEROIDS_MAX,
//   Asteroid())));
//
//   json asteroids_json;
//   status = read_json(mainfd, asteroids_json, -1);
//   if (!status) {
//     TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Asteroid>");
//     return false;
//   }
//
//   try {
//     asteroids = asteroids_json.template get<std::vector<Asteroid>>();
//     return true;
//   } catch (json::exception &ex) {
//     TraceLog(LOG_ERROR,
//              "JSON: Couldn't deserialize json into vector<Asteroid>");
//     return false;
//   }
// }

bool ClientNetworkManager::fetch_bullets(std::vector<Bullet> &bullets) {
  if (joinedRoom().room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch bullets");
    return false;
  }

  bool status;
  status = setEvent(mainfd, NetworkEvents::UpdateBullets);
  if (!status)
    return false;

  status = expectEvent(mainfd, NetworkEvents::UpdateBullets);
  if (!status)
    return false;

  json bullets_json = json(std::vector<Bullet>(
      Constants::BULLETS_PER_PLAYER * Constants::PLAYERS_MAX, Bullet()));
  status = read_json(mainfd, bullets_json, sizeof(bullets_json));
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Bullet>");
    return false;
  }

  try {
    bullets = bullets_json.template get<std::vector<Bullet>>();
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into vector<Bullet>");
    return false;
  }
}

bool ClientNetworkManager::shoot_bullet() {
  // Just return ok when msg sent
  return setEvent(mainfd, NetworkEvents::ShootBullets);

  // bool status;
  // status = setEvent(mainfd, NetworkEvents::ShootBullets);
  // if (!status)
  //   return false;
  // status = expectEvent(mainfd, NetworkEvents::ShootBullets);
  // if (!status)
  //   return false;
  //
  // uint32_t bullet_shot;
  // status = read_uint32(mainfd, bullet_shot);
  // if (!status) {
  //   TraceLog(LOG_ERROR, "NET: Couldn't receive whether bullet was shot");
  //   return false;
  // }
  //
  // status = (bullet_shot > 0);
  // if (!status) {
  //   TraceLog(LOG_ERROR,
  //            "GAME: Couldn't shoot a bullet according to the server");
  // }
  // return status;
}

bool ClientNetworkManager::vote_ready() {
  bool status;

  status = setEvent(mainfd, NetworkEvents::VoteReady);
  if (!status)
    return false;

  return status;
}

bool ClientNetworkManager::handle_end_round() {
  bool status;
  status = expectEvent(mainfd, NetworkEvents::EndRound);
  if (!status)
    return false;

  uint32_t winner_player_id;
  status = read_uint32(mainfd, winner_player_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive winner's player id");
  }

  gameManager().winner_player_id = winner_player_id;
  flip_game_manager();
  gameManager().winner_player_id = winner_player_id;

  return status;
}

// in lobby and room select only
bool ClientNetworkManager::handle_connection_check() {
  bool status;

  status = expectEvent(mainfd, NetworkEvents::CheckConnection);
  if (!status)
    return false;

  status = setEvent(mainfd, NetworkEvents::CheckConnection);
  return status;
}

void ClientNetworkManager::read_update_players() {
  json players_json;
  bool status = read_json(mainfd, players_json, -1);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of players");
    return;
  }
  try {
    auto players = players_json.template get<std::vector<Player>>();
    gameManager() = gameManagersPair.at(game_manager_draw_idx);
    gameManager().players = players;
    flip_game_manager();
    gameManager().players = players;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into vector<Player>");
    return;
  }
}

void ClientNetworkManager::handle_bullet_destroyed() {
  uint32_t bullet_id;
  bool status = read_uint32(mainfd, bullet_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive destroyed bullet id");
    return;
  }
  try {
    gameManager() = gameManagersPair.at(game_manager_draw_idx);
    gameManager().bullets.at(bullet_id).active = false;
    flip_game_manager();
    gameManager().bullets.at(bullet_id).active = false;
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_ERROR, "NET: bullet does not exist");
    return;
  }
}

void ClientNetworkManager::handle_player_destroyed() {
  uint32_t player_id;
  bool status = read_uint32(mainfd, player_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive destroyed player id");
    return;
  }
  try {
    gameManager() = gameManagersPair.at(game_manager_draw_idx);
    gameManager().players.at(player_id).active = false;
    flip_game_manager();
    gameManager().players.at(player_id).active = false;
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_ERROR, "NET: player does not exist");
    return;
  }
}

void ClientNetworkManager::handle_spawn_asteroid() {
  uint32_t id;
  bool status = read_uint32(mainfd, id);

  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive spawned Asteroid id");
    return;
  }

  json j;
  status = read_json(mainfd, j, -1);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive spawned Asteroid json");
    return;
  }

  Asteroid a;

  try {
    a = j;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into asteroid");
    return;
  }

  try {
    gameManager() = gameManagersPair.at(game_manager_draw_idx);
    gameManager().asteroids.at(id) = a;
    flip_game_manager();
    gameManager().asteroids.at(id) = a;
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_ERROR, "NET: Asteroid does not exist");
    return;
  }
}

void ClientNetworkManager::handle_asteroid_destroyed() {
  uint32_t id;
  bool status = read_uint32(mainfd, id);

  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive destroyed Asteroid id");
    return;
  }

  try {
    gameManager() = gameManagersPair.at(game_manager_draw_idx);
    gameManager().asteroids.at(id).active = false;
    flip_game_manager();
    gameManager().asteroids.at(id).active = false;
  } catch (const std::out_of_range &ex) {
    TraceLog(LOG_ERROR, "NET: Asteroid does not exist");
    return;
  }
}
