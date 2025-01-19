#include "graphicsManager.hpp"
#include "gameManager.hpp"
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
  win_font = font;
  player_font = font;
  // Optionally, set texture filtering for smooth font scaling
  SetTextureFilter(font.texture, TEXTURE_FILTER_ANISOTROPIC_16X);
}

GraphicsManager::~GraphicsManager() {
  UnloadTexture(player_texture);
  UnloadFont(font);
  // UnloadFont(win_font);
  // UnloadFont(player_font);
}

void GraphicsManager::DrawAsteroid(const Asteroid &asteroid) {
  DrawPolyLines(asteroid.position, asteroid.polygon, asteroid.size,
                asteroid.rotation, WHITE);
}

void GraphicsManager::DrawPlayer(const Player &player) {
  // DrawPoly(player.position, 3, 16, player.rotation, RED);
  // https://tradam.itch.io/raylib-drawtexturepro-interactive-demo
  // Rectangle source = {0, 0, float(player.texture.width),
  // float(player.texture.height)}; Rectangle dest = {player.position.x,
  // player.position.y, source.width, source.height}; Vector2 origin =
  // {dest.width/2.0f, dest.height/2.0f}; DrawCircle(player.position.x,
  // player.position.y, player.size/3, PINK); DrawTexturePro(player.texture,
  // source, dest, origin, player.rotation, player.player_color);
  const auto draw_offset =
      Vector2Scale(MeasureTextEx(font, Constants::PLAYER_AVATAR.c_str(),
                                 Constants::PLAYER_SIZE, 0),
                   0.5);
  DrawTextPro(player_font, Constants::PLAYER_AVATAR.c_str(), player.position,
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
                      Constants::screenHeight - 3 * Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

// void GraphicsManager::DrawTime(const GameManager &gm, const Room &r) {
//   auto time = steady_clock::now();
//   if (r.status == GameStatus::END_OF_ROUND)
//     time = gm.endRoundTime;
//
//   const char *text = TextFormat("%00.2f", time - gm.startRoundTime);
//   DrawTextPro(font, text,
//               Vector2{Constants::TEXT_OFFSET, Constants::TEXT_OFFSET},
//               Vector2{0, 0}, 0.0f, Constants::TEXT_SIZE,
//               Constants::TEXT_SPACING, RAYWHITE);
// }

// void GraphicsManager::DrawNewRoundCountdown(time_point<system_clock> when) {
//   auto time = duration_cast<seconds>(when - system_clock::now());
//   const char *text = TextFormat("New round in %d", (time));
//   Vector2 origin =
//       MeasureTextEx(font, text, Constants::TEXT_SIZE,
//       Constants::TEXT_SPACING);
//   DrawTextPro(
//       font, text,
//       Vector2{(float)Constants::screenWidth / 2, Constants::screenHeight},
//       Vector2{origin.x / 2, origin.y + Constants::TEXT_OFFSET}, 0,
//       Constants::TEXT_SIZE, Constants::TEXT_SPACING, RAYWHITE);
// }

void GraphicsManager::DrawTitle(const Room &r) {
  const char *text = TextFormat(
      "%s[%d/%d]",
      Constants::COOL_ROOM_NAMES[r.room_id - 1].c_str(), // FIXME: selected id
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

void GraphicsManager::DrawAsteroids(const GameManager &gm) {
  for (int i = 0; i < Constants::ASTEROIDS_MAX; i++) {
    if (!gm.asteroids[i].active)
      continue;
    DrawAsteroid(gm.asteroids[i]);
  }
}

void GraphicsManager::DrawPlayers(const GameManager &gm) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    DrawPlayer(gm.players[i]);
  }
}

void GraphicsManager::DrawBullets(const GameManager &gm) {
  for (int i = 0; i < Constants::PLAYERS_MAX * Constants::BULLETS_PER_PLAYER;
       i++) {
    if (!gm.bullets[i].active)
      continue;
    DrawBullet(gm.bullets[i]);
  }
}

void GraphicsManager::DrawBulletsGUI(const GameManager &gm,
                                     const uint32_t player_id) {
  int avaliable_bullets = 0;
  for (size_t i = player_id * Constants::BULLETS_PER_PLAYER;
       i < (player_id + 1) * Constants::BULLETS_PER_PLAYER; i++) {
    if (!gm.bullets[i].active)
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

void GraphicsManager::DrawWinnerText(const GameManager &gm) {
  if (gm.winner_player_id != UINT32_MAX) {
    const char *text = TextFormat(
        "%s PLAYER WIN", Constants::PLAYER_NAMES[gm.winner_player_id].c_str());
    Vector2 text_length = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                        Constants::TEXT_SPACING);
    // DrawRectangle(0, 0, Constants::screenWidth, Constants::screenHeight,
    //               Constants::BACKGROUND_COLOR_HALF_ALFA);
    DrawTextPro(
        win_font, text,
        Vector2{(float)Constants::screenWidth / 2, (float)text_length.y / 2},
        Vector2{text_length.x / 2, text_length.y / 2}, 0,
        Constants::TEXT_WIN_SIZE, Constants::TEXT_SPACING,
        Constants::PLAYER_COLORS[gm.winner_player_id]);
  }
}

void GraphicsManager::DrawRoomTitle() {
  const char *text1 = TextFormat("T\t\tNK BUSTERS");
  Vector2 origin = MeasureTextEx(player_font, text1, Constants::PLAYER_SIZE,
                                 Constants::TEXT_SPACING);
  DrawTextPro(player_font, text1, Vector2{(float)Constants::screenWidth / 2, 0},
              Vector2{origin.x / 2, -origin.y / 2}, 0, Constants::PLAYER_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
  const auto draw_offset =
      Vector2Scale(MeasureTextEx(font, Constants::PLAYER_AVATAR.c_str(),
                                 Constants::PLAYER_SIZE, 0),
                   0.5);
  DrawTextPro(player_font, Constants::PLAYER_AVATAR.c_str(),
              Vector2{origin.x / 2 - 3 * Constants::TEXT_OFFSET, origin.y},
              draw_offset, 10 * GetTime(), Constants::PLAYER_SIZE, 0,
              Constants::PLAYER_COLORS[0]);
}

void GraphicsManager::DrawRoomSubTitle() {
  const char *text = "ROOMS:";
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(
      player_font, text,
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
        "%c %s[%d/%d] - %s", active, Constants::COOL_ROOM_NAMES[i].c_str(),
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
