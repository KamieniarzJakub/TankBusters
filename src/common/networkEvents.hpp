#include <cstdint>
#include <string>

// underlining type is int, but is being sent over the
// network as uint32_t (after implicit conversion)
// I tried setting this to a typed enum with uint32_t,
// but it only caused more problems
enum NetworkEvents {
  // General
  NoEvent = 0,
  Disconnect = 10,
  CheckConnection = 20,
  EndRound = 30,
  StartRound = 40,
  NewGameSoon = 50,

  // Player
  GetClientId = 100,
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
const std::string network_event_to_string(uint32_t networkEvent);
