#pragma once
#include <vector>

enum PlayerInfo { NONE = 0, NOT_READY = 1, READY = 2 };

struct LobbyManager {
  int players_in_lobby;
  int ready_players;
  float new_round_timer;
  std::vector<int> players;

  LobbyManager();
  ~LobbyManager();

  void RestartLobby();

  void UpdatePlayers();

  bool UpdateLobbyStatus();
};
