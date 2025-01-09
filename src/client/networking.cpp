#include "networking.hpp"
#include "gameManager.hpp"
#include "jsonutils.hpp"
#include "networkEvents.hpp"
#include <cstdint>
#include <errno.h>
#include <nlohmann/json.hpp>
#include <room.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using json = nlohmann::json;

ClientNetworkManager::ClientNetworkManager(const char *host, const char *port) {
  fd = connect_to(host, port);
  client_id = get_new_client_id();
  // set_socket_timeout(5);
}

ClientNetworkManager::~ClientNetworkManager() { disconnect(); }

uint32_t ClientNetworkManager::get_new_client_id() {
  // Negotiate new client id
  write_uint32(fd, NetworkEvents::GetPlayerId);
  write_uint32(fd, client_id); // Get new id by sending id=0
  uint32_t event = read_uint32(fd);
  if (event == NetworkEvents::GetPlayerId) {
    uint32_t new_client_id = read_uint32(fd); // OK if >0
    return new_client_id;
  } else {
    return 0; // FIXME: THROW EXCEPTION
  }
}

std::vector<Room> ClientNetworkManager::get_rooms() {
  write_uint32(fd, NetworkEvents::GetRoomList);
  uint32_t _ = read_uint32(fd); // FIXME:
  json rooms = read_json(fd);
  return rooms.template get<std::vector<Room>>();
}

void ClientNetworkManager::disconnect() {
  shutdown(fd, SHUT_RDWR);
  close(fd);
  // if (status != 0) {
  //   error(1, errno, "Couldn't shutdown the connection properly\n");
  // }
  //
  // if (status != 0) {
  //   error(1, errno, "Couldn't close the socket properly\n");
  // }
}

void ClientNetworkManager::set_socket_timeout(time_t seconds) {
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
}

int ClientNetworkManager::connect_to(const char *host, const char *port) {
  int status, fd;
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  struct addrinfo *p_addr = nullptr;
  struct addrinfo *c_addr = nullptr;
  status = getaddrinfo(host, port, &hints, &p_addr);
  if (status != 0 || p_addr == nullptr) {
    error(1, errno, "Couldn't resolve hostname or service\n");
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
    error(1, errno, "Could not connect\n");
  }

  return fd;
}

bool ClientNetworkManager::join_room(uint32_t room_id) {
  // FIXME: UNFINISHED
  write_uint32(fd, NetworkEvents::JoinRoom);
  write_uint32(fd, room_id);

  uint32_t event = read_uint32(fd);
  if (event == NetworkEvents::JoinRoom) {
    uint32_t joined_room = read_uint32(fd);
    return true;
  } else {
    return false;
  }
}

Room ClientNetworkManager::fetch_room_state() {
  return fetch_room_state(this->room_id);
}

Room ClientNetworkManager::fetch_room_state(uint32_t fetch_room_id) {
  // FIXME: UNFINISHED
  //
  // UpdateRoomState
  // UpdateRoomState
  return {};
}

void ClientNetworkManager::leave_room() {
  if (room_id == 0) {
    return;
  }
  // FIXME: UNFINISHED
  //
  //   LeaveRoom
  //   LeaveRoom
  room_id = 0;
}

// position, rotation, velocity
bool ClientNetworkManager::send_movement(Vector2 direction) {
  // FIXME: UNFINISHED
  //
  // PlayerMovement
  // PlayerMovement
  return false;
}

GameManager ClientNetworkManager::fetch_game_state() {
  // FIXME: UNFINISHED
  //
  // UpdateGameState,
  // UpdateGameState,
  return {};
}

std::vector<Player> ClientNetworkManager::fetch_players() {
  // FIXME: UNFINISHED
  //
  // UpdatePlayers
  // UpdatePlayers
  return {};
}

std::vector<Asteroid> ClientNetworkManager::fetch_asteroids() {
  // FIXME: UNFINISHED
  //
  // UpdateAsteroids
  // UpdateAsteroids
  return {};
}

std::vector<Bullet> ClientNetworkManager::fetch_bullets() {
  // FIXME: UNFINISHED
  //
  // UpdateBullets
  // UpdateBullets
  return {};
}

// TODO: CHANGE TO CREATE_NEW_BULLET
// returns isPlayerShootingAccoringToServer
// flips player shooting state according to the server
bool ClientNetworkManager::shoot_bullets() {
  // FIXME: UNFINISHED
  //
  // FlipShooting
  // FlipShooting
  return {};
}

int ClientNetworkManager::vote_ready() {
  // FIXME: UNFINISHED
  //
  // VoteReady,
  // VoteReady,

  return {};
}

// send END ROUND
// send who won
void ClientNetworkManager::handle_end_round() {
  // FIXME: UNFINISHED
  //
  // recv only EndRound
}

// in lobby and room select only
bool ClientNetworkManager::handle_connection_check() {
  // FIXME: UNFINISHED
  //
  // CheckConnection
  // CheckConnection
  return false;
}
