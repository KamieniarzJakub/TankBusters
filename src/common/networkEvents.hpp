enum NetworkEvents {
  // General
  NoEvent = 0,
  Disconnect = 10,
  CheckConnection = 20,
  EndRound = 30,

  // Player
  GetPlayerId = 100,
  VoteReady = 110,
  PlayerMovement = 120,
  FlipShooting = 130,

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
