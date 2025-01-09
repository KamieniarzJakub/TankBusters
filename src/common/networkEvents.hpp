#include <string>

enum NetworkEvents {
  // General
  NoEvent = 0,
  Disconnect = 10,
  CheckConnection = 20,
  EndRound = 30,

  // Player
  GetClientId = 100, // TODO: rename to GetClientId
  VoteReady = 110,
  PlayerMovement = 120,
  ShootBullets = 130,

  // Room
  GetRoomList = 200,
  JoinRoom = 220,
  LeaveRoom = 230,

  // Data fetching
  UpdateGameState = 300,
  UpdateRoomState = 310,
  UpdatePlayers = 320,
  UpdateAsteroids = 330,
  UpdateBullets = 340,
};

const std::string network_event_to_string(NetworkEvents networkEvent);
