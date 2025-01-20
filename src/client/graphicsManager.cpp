#include "graphicsManager.hpp"
#include "player.hpp"
#include "resource_dir.hpp"
#include "room.hpp"
#include <chrono>
#include <cstdint>
#include <raylib.h>

GraphicsManager::GraphicsManager() {
  // Utility function from resource_dir.h to find the resources folder and set
  // it as the current working directory so we can load from it
  SearchAndSetResourceDir("resources");

  // Load a texture from the resources directory
  player_texture = LoadTexture("Player.png");

  font = LoadFontEx(Constants::ROBOTO_REGULAR.c_str(), Constants::TEXT_SIZE,
                    nullptr, 0);
  SetTextureFilter(font.texture, TEXTURE_FILTER_ANISOTROPIC_16X);
}

GraphicsManager::~GraphicsManager() {
  UnloadTexture(player_texture);
  UnloadFont(font);
}

void GraphicsManager::DrawAsteroid(const Asteroid &asteroid) {
  DrawPolyLines(asteroid.position, asteroid.polygon, asteroid.size,
                asteroid.rotation, WHITE);
}

void GraphicsManager::DrawPlayer(const Player &player) {
  const auto draw_offset =
      Vector2Scale(MeasureTextEx(font, Constants::PLAYER_AVATAR.c_str(),
                                 Constants::PLAYER_SIZE, 0),
                   0.5);
  DrawTextPro(font, Constants::PLAYER_AVATAR.c_str(), player.position,
              draw_offset, player.rotation + 90.0f, Constants::PLAYER_SIZE, 0,
              player.player_color);
}

void GraphicsManager::DrawBullet(const Bullet &bullet) {
  DrawCircle(bullet.position.x, bullet.position.y, Constants::BULLET_SIZE,
             WHITE);
}

void GraphicsManager::DrawTimer(const char *message,
                                time_point<system_clock> when) {
  auto time = duration_cast<seconds>(when - system_clock::now()).count();
  const char *text = TextFormat("%s %ld", message, time);
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight - 5 * Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawTitle(const Room &r) {
  const char *text =
      TextFormat("%s[%d/%d]", r.name.c_str(),
                 r.players.size() - get_X_players(r.players, PlayerInfo::NONE),
                 Constants::PLAYERS_MAX);
  Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                 Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2, origin.y / 2},
              Vector2{origin.x / 2, -origin.y / 3}, 0, Constants::TEXT_WIN_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawLobbyPlayers(const Room &r) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    float margin = (i - 2) * 2 * Constants::TEXT_OFFSET;
    const char *text =
        TextFormat("%s PLAYER", Constants::PLAYER_NAMES[i].c_str());
    Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_SIZE,
                                   Constants::TEXT_SPACING);
    Color player_color = (r.players[i].state == PlayerInfo::NONE)
                             ? Constants::NOT_CONNECTED_GRAY
                             : Constants::PLAYER_COLORS[i];
    if (r.players[i].state == PlayerInfo::NOT_READY)
      player_color.a = 50;
    DrawTextPro(font, text,
                Vector2{(float)Constants::screenWidth / 2,
                        (float)Constants::screenHeight / 2 + margin},
                {origin.x / 2, origin.y / 2}, 0, Constants::TEXT_SIZE,
                Constants::TEXT_SPACING, player_color);
  }
}

void GraphicsManager::DrawReadyMessage() {
  const char *text = "Click [Space] to sign up for the next game";
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight - Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawExitLobbyMessage() {
  const char *text = "Click [Enter] to exit lobby";
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight - 3 * Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawAsteroids(const std::vector<Asteroid> &asteroids) {
  for (auto &asteroid : asteroids) {
    if (!asteroid.active)
      continue;
    DrawAsteroid(asteroid);
  }
}

void GraphicsManager::DrawPlayers(const std::vector<Player> &players) {
  for (auto &player : players) {
    DrawPlayer(player);
  }
}

void GraphicsManager::DrawBullets(const std::vector<Bullet> &bullets) {
  for (auto &bullet : bullets) {
    if (!bullet.active)
      continue;
    DrawBullet(bullet);
  }
}

void GraphicsManager::DrawBulletsGUI(const std::vector<Bullet> &bullets,
                                     const uint32_t player_id) {
  int avaliable_bullets = 0;
  for (size_t i = player_id * Constants::BULLETS_PER_PLAYER;
       i < (player_id + 1) * Constants::BULLETS_PER_PLAYER; i++) {

    if (!bullets[i].active)
      avaliable_bullets++;
  }
  std::string bullets_text =
      avaliable_bullets ? std::string(avaliable_bullets, '*') : "EMPTY";
  const char *text = TextFormat("%s", bullets_text.c_str());
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{Constants::screenWidth - Constants::TEXT_OFFSET,
                      Constants::screenHeight - Constants::TEXT_OFFSET},
              Vector2{origin.x, origin.y / 2}, 0.0f, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawWinnerText(const uint32_t winner_player_id) {
  if (winner_player_id != UINT32_MAX) {
    const char *text = TextFormat(
        "%s PLAYER WIN", Constants::PLAYER_NAMES[winner_player_id].c_str());
    Vector2 text_length = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                        Constants::TEXT_SPACING);
    DrawTextPro(
        font, text,
        Vector2{(float)Constants::screenWidth / 2, (float)text_length.y / 2},
        Vector2{text_length.x / 2, text_length.y / 2}, 0,
        Constants::TEXT_WIN_SIZE, Constants::TEXT_SPACING,
        Constants::PLAYER_COLORS[winner_player_id]);
  }
}

void GraphicsManager::DrawRoomTitle() {
  const char *text1 = TextFormat("T\t\tNK BUSTERS");
  Vector2 origin = MeasureTextEx(font, text1, Constants::PLAYER_SIZE,
                                 Constants::TEXT_SPACING);
  DrawTextPro(font, text1, Vector2{(float)Constants::screenWidth / 2, 0},
              Vector2{origin.x / 2, -origin.y / 2}, 0, Constants::PLAYER_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
  const auto draw_offset =
      Vector2Scale(MeasureTextEx(font, Constants::PLAYER_AVATAR.c_str(),
                                 Constants::PLAYER_SIZE, 0),
                   0.5);
  DrawTextPro(font, Constants::PLAYER_AVATAR.c_str(),
              Vector2{origin.x / 2 - 3 * Constants::TEXT_OFFSET, origin.y},
              draw_offset, 10 * GetTime(), Constants::PLAYER_SIZE, 0,
              Constants::PLAYER_COLORS[0]);
}

void GraphicsManager::DrawRoomSubTitle() {
  const char *text = "ROOMS:";
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(
      font, text,
      Vector2{(float)Constants::screenWidth / 2,
              (float)Constants::screenHeight / 2 - 2 * Constants::TEXT_OFFSET},
      Vector2{origin.x / 2, origin.y / 2}, 0, Constants::TEXT_SIZE,
      Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawRoomBottomText() {
  const char *text = "Click [space] to join selected room";
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight - Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawRooms(const std::vector<Room> &rooms,
                                uint32_t selected_room_index_not_id) {
  uint32_t i = 0;
  for (const auto &rp : rooms) {
    const auto &room = rp;
    std::string room_status =
        (room.status == GameStatus::GAME) ? "IN GAME" : "LOBBY";
    Color room_color = Constants::NOT_CONNECTED_GRAY;
    float text_size = Constants::TEXT_SIZE;
    char active = ' ';
    if (selected_room_index_not_id == i) {
      room_color = Constants::PLAYER_COLORS[0];
      text_size *= 1.2;
      active = '>';
    }

    if (get_X_players(room.players, PlayerInfo::NONE) == 0) {
      room_status = "FULL";
      room_color.a = 50;
    }
    float margin = i * 2 * Constants::TEXT_OFFSET;
    const char *text = TextFormat(
        "%c %s[%d/%d] - %s", active, rooms.at(i).name.c_str(),
        room.players.size() - get_X_players(room.players, PlayerInfo::NONE),
        Constants::PLAYERS_MAX, room_status.c_str());
    Vector2 origin =
        MeasureTextEx(font, text, text_size, Constants::TEXT_SPACING);

    DrawTextPro(font, text,
                Vector2{(float)Constants::screenWidth / 2,
                        (float)Constants::screenHeight / 2 + margin},
                {origin.x / 2, origin.y / 2}, 0, text_size,
                Constants::TEXT_SPACING, room_color);
    i++;
  }
}
