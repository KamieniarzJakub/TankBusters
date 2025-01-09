#include <cstdint>
#include <networkEvents.hpp>

const std::string network_event_to_string(uint32_t networkEvent) {
  return network_event_to_string((NetworkEvents)networkEvent);
}

const std::string network_event_to_string(NetworkEvents networkEvent) {
  switch (networkEvent) {
  case NoEvent:
    return "NoEvent";
  case Disconnect:
    return "Disconnect";
  case CheckConnection:
    return "CheckConnection";
  case EndRound:
    return "EndRound";
  case GetClientId:
    return "GetClientIdId";
  case VoteReady:
    return "VoteReady";
  case PlayerMovement:
    return "PlayerMovement";
  case ShootBullets:
    return "ShootBullets";
  case GetRoomList:
    return "GetRoomList";
  case JoinRoom:
    return "JoinRoom";
  case LeaveRoom:
    return "LeaveRoom";
  case UpdateGameState:
    return "UpdateGameState";
  case UpdateRoomState:
    return "UpdateRoomState";
  case UpdatePlayers:
    return "UpdatePlayers";
  case UpdateAsteroids:
    return "UpdateAsteroids";
  case UpdateBullets:
    return "UpdateBullets";
  default:
    return "Unknown";
  }
}
