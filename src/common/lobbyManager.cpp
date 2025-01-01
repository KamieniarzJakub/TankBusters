#include "lobbyManager.hpp"
#include "constants.hpp"

LobbyManager::LobbyManager() { RestartLobby(); }

LobbyManager::~LobbyManager() {}

void LobbyManager::RestartLobby() {
  players_in_lobby = Constants::PLAYERS_MAX; // POTEM TRZEBA ZMIENIĆ
  players = std::vector<int>(Constants::PLAYERS_MAX, 0);
  new_round_timer = -1;
  ready_players = 0;
}

void LobbyManager::UpdatePlayers() {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    if (IsKeyPressed(KEY_SPACE)) {
      ++players[i] %= 3;
      if (players[i] == PlayerInfo::READY)
        ready_players++;
      else if (!players[i])
        ready_players--;
    }
  }
  // TraceLog(LOG_DEBUG, "PLAYERS READY FOR NEW ROUND: %d", ready_players);
}

bool LobbyManager::UpdateLobbyStatus() {
  if (ready_players > 2) {
    new_round_timer = new_round_timer > 0 ? new_round_timer : GetTime();
    if (new_round_timer > 0 &&
        int(GetTime() - new_round_timer) >= Constants::LOBBY_READY_TIME)
      return true;
  } else
    new_round_timer = -1;
  return false;
}
