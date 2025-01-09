#include "networking.hpp"
#include "gameManager.hpp"
#include "jsonutils.hpp"
#include "raylib.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <room.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using json = nlohmann::json;

ClientNetworkManager::ClientNetworkManager(const char *host, const char *port) {
  fd = connect_to(host, port);

  if (!get_new_client_id(client_id)) {
    TraceLog(LOG_ERROR, "GAME: Could not get a new client id");
  }
  // set_socket_timeout(5);
}

ClientNetworkManager::~ClientNetworkManager() { disconnect(); }

bool ClientNetworkManager::get_new_client_id(uint32_t &client_id) {
  bool status;

  // Negotiate new client id
  status = write_uint32(fd, NetworkEvents::GetClientId);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Cannot send GetClientId NetworkEvent");
    return false;
  }

  // Get new id by sending id=0
  status = write_uint32(fd, client_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Cannot send client_id");
    return false;
  }

  status = expectEvent(fd, NetworkEvents::GetClientId);
  if (!status) {
    return false;
  }

  uint32_t new_client_id;
  status = read_uint32(fd, new_client_id);
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

bool ClientNetworkManager::get_rooms(std::vector<Room> &rooms) {
  bool status;
  status = write_uint32(fd, NetworkEvents::GetRoomList);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Cannot send GetRoomList NetworkEvent");
    return false;
  }

  status = expectEvent(fd, NetworkEvents::GetRoomList);
  if (!status) {
    return false;
  }

  json rooms_json;
  status = read_json(fd, rooms_json, -1); // TODO: replace -1 with real max
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Rooms>");
    return false;
  }

  try {
    rooms = rooms_json.template get<std::vector<Room>>();
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't deserialize json into vector<Room>");
    return false;
  }
}

// FIXME: WHY SERVER CANNOT READ THIS EVENT
int ClientNetworkManager::disconnect() {
  int status;
  status = setEvent(fd, NetworkEvents::Disconnect);
  if (!status) {
    return false;
  }

  status = shutdown(fd, SHUT_RDWR);
  if (status != 0) {
    TraceLog(LOG_ERROR, "Couldn't shutdown the connection properly");
    return status;
  }
  status = close(fd);
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
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
  if (status != 0) {
    return status;
  }
  return setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);
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

// Join a room by room_id
// returns true if joined room
bool ClientNetworkManager::join_room(uint32_t room_id) {
  bool status;

  status = setEvent(fd, NetworkEvents::JoinRoom);
  if (!status) {
    return false;
  }

  if (room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into join_room");
    return false;
  }

  status = write_uint32(fd, room_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send Room ID");
    return false;
  }

  status = expectEvent(fd, NetworkEvents::JoinRoom);
  if (!status) {
    return false;
  }

  uint32_t joined_room;
  status = read_uint32(fd, joined_room);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't read joined room id");
    return false;
  }
  return (joined_room == room_id);
}

bool ClientNetworkManager::fetch_room_state(Room &room) {
  return fetch_room_state(this->room_id, room);
}

bool ClientNetworkManager::fetch_room_state(uint32_t fetch_room_id,
                                            Room &room) {
  TraceLog(LOG_INFO, "%lu %lu", fetch_room_id, room.room_id);
  // FIXME: UNFINISHED
  //
  // UpdateRoomState
  // UpdateRoomState
  return false;
}

bool ClientNetworkManager::leave_room() {
  if (room_id == 0) {
    return false;
  }
  // FIXME: UNFINISHED
  //
  //   LeaveRoom
  //   LeaveRoom
  room_id = 0;
  return false;
}

bool ClientNetworkManager::send_movement(Vector2 position, Vector2 velocity,
                                         float rotation) {
  // FIXME: UNFINISHED
  //
  // PlayerMovement
  // PlayerMovement
  return false;
}

bool ClientNetworkManager::fetch_game_state(GameManager &gameManager) {
  // FIXME: UNFINISHED
  //
  // UpdateGameState,
  // UpdateGameState,
  return false;
}

bool ClientNetworkManager::fetch_players(std::vector<Player> &players) {
  // FIXME: UNFINISHED
  //
  // UpdatePlayers
  // UpdatePlayers
  return false;
}

bool ClientNetworkManager::fetch_asteroids(std::vector<Asteroid> &asteroids) {
  // FIXME: UNFINISHED
  //
  // UpdateAsteroids
  // UpdateAsteroids
  return false;
}

bool ClientNetworkManager::fetch_bullets(std::vector<Bullet> &bullets) {
  // FIXME: UNFINISHED
  //
  // UpdateBullets
  // UpdateBullets
  return false;
}

// TODO: CHANGE TO CREATE_NEW_BULLET
bool ClientNetworkManager::shoot_bullet() {
  // FIXME: UNFINISHED
  //
  // ShootBullets:
  // ShootBullets:
  return false;
}

bool ClientNetworkManager::vote_ready(uint32_t &ready_players) {
  // FIXME: UNFINISHED
  //
  // VoteReady,
  // VoteReady,

  return false;
}

// send END ROUND
// send who won
bool ClientNetworkManager::handle_end_round(uint32_t &winner_player_id) {
  // FIXME: UNFINISHED
  //
  // recv only EndRound
  return false;
}

// in lobby and room select only
bool ClientNetworkManager::handle_connection_check() {
  // FIXME: UNFINISHED
  //
  // CheckConnection
  // CheckConnection
  return false;
}
