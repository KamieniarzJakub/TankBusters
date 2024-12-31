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

void LobbyManager::DrawTimer(Font font) {
  if (new_round_timer > 0) {
    int time = Constants::LOBBY_READY_TIME - int(GetTime() - new_round_timer);
    const char *text = TextFormat("New round in %d", int(time));
    Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_SIZE,
                                   Constants::TEXT_SPACING);
    DrawTextPro(font, text,
                Vector2{(float)Constants::screenWidth / 2,
                        Constants::screenHeight - 2 * Constants::TEXT_OFFSET},
                Vector2{origin.x / 2, 2 * origin.y}, 0, Constants::TEXT_SIZE,
                Constants::TEXT_SPACING, RAYWHITE);
  }
}

void LobbyManager::DrawTitle(Font font) {
  const char *text =
      TextFormat("LOBBY[%d/%d]", players_in_lobby, Constants::PLAYERS_MAX);
  Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                 Constants::TEXT_SPACING);
  DrawTextPro(font, text, Vector2{(float)Constants::screenWidth / 2, 0},
              Vector2{origin.x / 2, -origin.y / 3}, 0, Constants::TEXT_WIN_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void LobbyManager::DrawLobbyPlayers(Font font) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    float margin = (i - 2) * 2 * Constants::TEXT_OFFSET;
    const char *text =
        TextFormat("%s PLAYER", Constants::PLAYER_NAMES[i].c_str());
    Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_SIZE,
                                   Constants::TEXT_SPACING);
    Color player_color = (players[i] == PlayerInfo::NONE)
                             ? Constants::NOT_CONNECTED_GRAY
                             : Constants::PLAYER_COLORS[i];
    if (players[i] == PlayerInfo::NOT_READY)
      player_color.a = 50;
    DrawTextPro(font, text,
                Vector2{(float)Constants::screenWidth / 2,
                        (float)Constants::screenHeight / 2 + margin},
                {origin.x / 2, origin.y / 2}, 0, Constants::TEXT_SIZE,
                Constants::TEXT_SPACING, player_color);
  }
}

void LobbyManager::DrawReadyMessage(Font font) {
  const char *text = "Click [Space] to sign up for the next game";
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight - Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}
