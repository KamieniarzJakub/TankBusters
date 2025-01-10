#include "networking.hpp"
#include "asteroid.hpp"
#include "bullet.hpp"
#include "constants.hpp"
#include "gameManager.hpp"
#include "jsonutils.hpp"
#include "player.hpp"
#include "raylib.h"
#include "vec2json.hpp"
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
// NOTE: REMEMBER TO UPDATE VALUE IN GAME MANAGER
//
// returns true if joined room
bool ClientNetworkManager::join_room(uint32_t join_room_id) {
  bool status;

  status = setEvent(fd, NetworkEvents::JoinRoom);
  if (!status) {
    return false;
  }

  if (join_room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into join_room");
    return false;
  }

  status = write_uint32(fd, join_room_id);
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

  if (joined_room == join_room_id) {
    this->room_id = joined_room;
    return true;
  } else {
    return false;
  }
}

// Get info about currently joined room
bool ClientNetworkManager::fetch_room_state(Room &room) {
  return fetch_room_state(this->room_id, room);
}

// Get info about arbitrary room by room id
bool ClientNetworkManager::fetch_room_state(uint32_t fetch_room_id,
                                            Room &room) {
  if (fetch_room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into join_room");
    return false;
  }

  bool status;
  status = setEvent(fd, NetworkEvents::UpdateRoomState);
  if (!status)
    return false;

  status = write_uint32(fd, fetch_room_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
    return false;
  }

  status = expectEvent(fd, NetworkEvents::UpdateRoomState);
  if (!status)
    return false;

  json room_json = json(Room());
  status = read_json(fd, room_json, sizeof(room_json));
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
  if (this->room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Leaving Room without previously joining any");
    return false;
  }

  bool status;
  status = setEvent(fd, NetworkEvents::LeaveRoom);
  if (!status) {
    return false;
  }

  status = expectEvent(fd, NetworkEvents::LeaveRoom);
  if (!status) {
    return false;
  }

  this->room_id = 0;
  return true;
}

// NOTE: Potentially could be streamed
bool ClientNetworkManager::send_movement(Vector2 position, Vector2 velocity,
                                         float rotation) {
  // TODO: bounds check
  json movement;
  try {
    movement = {
        {"position", position}, {"velocity", velocity}, {"rotation", rotation}};
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR, "JSON: Couldn't serialize movement into json");
    return false;
  }

  bool status = setEvent(fd, NetworkEvents::PlayerMovement);
  if (!status)
    return false;

  status = write_json(fd, movement);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't send json of movement");
    return false;
  }

  // Intentionally no response

  return true;
}

// Fetches GameManager state for currently joined room
// Players' state, asteroids and bullets are *NOT* fetched
bool ClientNetworkManager::fetch_game_state(GameManager &gameManager) {

  if (room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch game state");
    return false;
  }

  bool status;
  status = setEvent(fd, NetworkEvents::UpdateGameState);
  if (!status)
    return false;

  // status = write_uint32(fd, room_id);
  // if (!status) {
  //   TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
  //   return false;
  // }

  status = expectEvent(fd, NetworkEvents::UpdateGameState);
  if (!status)
    return false;

  json game_json = json(GameManager());
  status = read_json(fd, game_json, sizeof(game_json));
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
  if (room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch players");
    return false;
  }

  bool status;
  status = setEvent(fd, NetworkEvents::UpdatePlayers);
  if (!status)
    return false;

  // TODO: Possibly unnecessary
  // status = write_uint32(fd, room_id);
  // if (!status) {
  //   TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
  //   return false;
  // }

  status = expectEvent(fd, NetworkEvents::UpdatePlayers);
  if (!status)
    return false;

  json players_json =
      json(std::vector<Player>(Constants::PLAYERS_MAX, Player()));
  status = read_json(fd, players_json, sizeof(players_json));
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

bool ClientNetworkManager::fetch_asteroids(std::vector<Asteroid> &asteroids) {
  if (room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch asteroids");
    return false;
  }

  bool status;
  status = setEvent(fd, NetworkEvents::UpdateAsteroids);
  if (!status)
    return false;

  // TODO: Possibly unnecessary
  // status = write_uint32(fd, room_id);
  // if (!status) {
  //   TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
  //   return false;
  // }

  status = expectEvent(fd, NetworkEvents::UpdateAsteroids);
  if (!status)
    return false;

  json asteroids_json =
      json(std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid()));
  status = read_json(fd, asteroids_json, sizeof(asteroids_json));
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive json of vector<Asteroid>");
    return false;
  }

  try {
    asteroids = asteroids_json.template get<std::vector<Asteroid>>();
    return true;
  } catch (json::exception &ex) {
    TraceLog(LOG_ERROR,
             "JSON: Couldn't deserialize json into vector<Asteroid>");
    return false;
  }
}

bool ClientNetworkManager::fetch_bullets(std::vector<Bullet> &bullets) {
  if (room_id == 0) {
    TraceLog(LOG_ERROR, "GAME: Room ID=0 passed into fetch bullets");
    return false;
  }

  bool status;
  status = setEvent(fd, NetworkEvents::UpdateBullets);
  if (!status)
    return false;

  // TODO: Possibly unnecessary
  // status = write_uint32(fd, room_id);
  // if (!status) {
  //   TraceLog(LOG_ERROR, "NET: Couldn't send ROOM ID");
  //   return false;
  // }

  status = expectEvent(fd, NetworkEvents::UpdateBullets);
  if (!status)
    return false;

  json bullets_json = json(std::vector<Bullet>(
      Constants::BULLETS_PER_PLAYER * Constants::PLAYERS_MAX, Bullet()));
  status = read_json(fd, bullets_json, sizeof(bullets_json));
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
  bool status;
  status = setEvent(fd, NetworkEvents::ShootBullets);
  if (!status)
    return false;

  status = expectEvent(fd, NetworkEvents::ShootBullets);
  if (!status)
    return false;

  uint32_t bullet_shot;
  status = read_uint32(fd, bullet_shot);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive whether bullet was shot");
    return false;
  }

  status = (bullet_shot > 0);
  if (!status) {
    TraceLog(LOG_ERROR,
             "GAME: Couldn't shoot a bullet according to the server");
  }
  return status;
}

bool ClientNetworkManager::vote_ready(uint32_t &ready_players) {
  bool status;

  status = setEvent(fd, NetworkEvents::VoteReady);
  if (!status)
    return false;

  status = expectEvent(fd, NetworkEvents::VoteReady);
  if (!status)
    return false;

  status = read_uint32(fd, ready_players);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive ready players");
  }

  return status;
}

bool ClientNetworkManager::handle_end_round(uint32_t &winner_player_id) {
  bool status;
  status = expectEvent(fd, NetworkEvents::EndRound);
  if (!status)
    return false;

  status = read_uint32(fd, winner_player_id);
  if (!status) {
    TraceLog(LOG_ERROR, "NET: Couldn't receive winner's player id");
  }

  return status;
}

// in lobby and room select only
bool ClientNetworkManager::handle_connection_check() {
  bool status;

  status = expectEvent(fd, NetworkEvents::CheckConnection);
  if (!status)
    return false;

  status = setEvent(fd, NetworkEvents::CheckConnection);
  return status;
}
