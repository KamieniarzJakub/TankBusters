#include <cstdint>
#include <networkEvents.hpp>

const std::string network_event_to_string(uint32_t networkEvent) {
  int val = (int)networkEvent;
  return network_event_to_string((NetworkEvents)val);
}

const std::string network_event_to_string(NetworkEvents networkEvent) {
  switch (networkEvent) {
  case NetworkEvents::NoEvent:
    return "NoEvent";
  case NetworkEvents::Disconnect:
    return "Disconnect";
  case NetworkEvents::CheckConnection:
    return "CheckConnection";
  case NetworkEvents::EndRound:
    return "EndRound";
  case NetworkEvents::StartRound:
    return "StartRound";
  case NetworkEvents::GetClientId:
    return "GetClientId";
  case NetworkEvents::VoteReady:
    return "VoteReady";
  case NetworkEvents::PlayerMovement:
    return "PlayerMovement";
  case NetworkEvents::ShootBullets:
    return "ShootBullets";
  case NetworkEvents::GetRoomList:
    return "GetRoomList";
  case NetworkEvents::JoinRoom:
    return "JoinRoom";
  case NetworkEvents::LeaveRoom:
    return "LeaveRoom";
  case NetworkEvents::UpdateGameState:
    return "UpdateGameState";
  case NetworkEvents::UpdateRoomState:
    return "UpdateRoomState";
  case NetworkEvents::UpdatePlayers:
    return "UpdatePlayers";
  case NetworkEvents::UpdateAsteroids:
    return "UpdateAsteroids";
  case NetworkEvents::UpdateBullets:
    return "UpdateBullets";
  default:
    return "Unknown";
  }
}
