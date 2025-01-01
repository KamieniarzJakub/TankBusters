#include "graphicsManager.hpp"
#include "gameManager.hpp"
#include "lobbyManager.hpp"
#include "resource_dir.hpp"
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
  UnloadFont(win_font);
  UnloadFont(player_font);
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

void GraphicsManager::DrawTimer(const LobbyManager &lm) {
  if (lm.new_round_timer > 0) {
    int time =
        Constants::LOBBY_READY_TIME - int(GetTime() - lm.new_round_timer);
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

void GraphicsManager::DrawTitle(const LobbyManager &lm) {
  const char *text =
      TextFormat("LOBBY[%d/%d]", lm.players_in_lobby, Constants::PLAYERS_MAX);
  Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                 Constants::TEXT_SPACING);
  DrawTextPro(font, text, Vector2{(float)Constants::screenWidth / 2, 0},
              Vector2{origin.x / 2, -origin.y / 3}, 0, Constants::TEXT_WIN_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawLobbyPlayers(const LobbyManager &lm) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    float margin = (i - 2) * 2 * Constants::TEXT_OFFSET;
    const char *text =
        TextFormat("%s PLAYER", Constants::PLAYER_NAMES[i].c_str());
    Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_SIZE,
                                   Constants::TEXT_SPACING);
    Color player_color = (lm.players[i] == PlayerInfo::NONE)
                             ? Constants::NOT_CONNECTED_GRAY
                             : Constants::PLAYER_COLORS[i];
    if (lm.players[i] == PlayerInfo::NOT_READY)
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

void GraphicsManager::DrawTime(const GameManager &gm, double time) {
  if (gm.status == Status::END_OF_ROUND)
    time = gm.endRoundTime;

  const char *text = TextFormat("%00.2f", time - gm.startRoundTime);
  DrawTextPro(font, text,
              Vector2{Constants::TEXT_OFFSET, Constants::TEXT_OFFSET},
              Vector2{0, 0}, 0.0f, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawWinnerText(const GameManager &gm) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    if (gm.players[i].active) {
      const char *text =
          TextFormat("%s PLAYER WIN", Constants::PLAYER_NAMES[i].c_str());
      Vector2 text_length = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                          Constants::TEXT_SPACING);
      DrawRectangle(0, 0, Constants::screenWidth, Constants::screenHeight,
                    Constants::BACKGROUND_COLOR_HALF_ALFA);
      DrawTextPro(win_font, text,
                  Vector2{(float)Constants::screenWidth / 2,
                          (float)Constants::screenHeight / 2},
                  Vector2{text_length.x / 2, text_length.y / 2}, 0,
                  Constants::TEXT_WIN_SIZE, Constants::TEXT_SPACING,
                  Constants::PLAYER_COLORS[i]);
      return;
    }
  }
}

void GraphicsManager::DrawNewRoundCountdown(const GameManager &gm) {
  int time = Constants::NEW_ROUND_WAIT_TIME - int(GetTime() - gm.endRoundTime);
  const char *text = TextFormat("New round in %d", time);
  Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(
      font, text,
      Vector2{(float)Constants::screenWidth / 2, Constants::screenHeight},
      Vector2{origin.x / 2, origin.y + Constants::TEXT_OFFSET}, 0,
      Constants::TEXT_SIZE, Constants::TEXT_SPACING, RAYWHITE);
}
