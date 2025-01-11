#include "graphicsManager.hpp"
#include "gameManager.hpp"
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

void GraphicsManager::DrawTimer(const GameManager &gm) {
    int time =
        Constants::LOBBY_READY_TIME - int(GetTime() - gm.new_round_timer);
    const char *text = TextFormat("New round in %d", int(time));
    Vector2 origin =
      MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
  DrawTextPro(font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight - 3*Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawTitle(const GameManager &gm) {
  const char *text =
      TextFormat("%s[%d/%d]", Constants::COOL_ROOM_NAMES[0].c_str(), gm.players.size(), Constants::PLAYERS_MAX); //FIXME: const char*
  Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE,
                                 Constants::TEXT_SPACING);
  DrawTextPro(font, text, Vector2{(float)Constants::screenWidth / 2, 0},
              Vector2{origin.x / 2, -origin.y / 3}, 0, Constants::TEXT_WIN_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}


void GraphicsManager::DrawLobbyPlayers(const GameManager &gm) {
  for (int i = 0; i < Constants::PLAYERS_MAX; i++) {
    float margin = (i - 2) * 2 * Constants::TEXT_OFFSET;
    const char *text =
        TextFormat("%s PLAYER", Constants::PLAYER_NAMES[i].c_str());
    Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_SIZE,
                                   Constants::TEXT_SPACING);
    Color player_color = (gm.players[i].state == PlayerInfo::NONE)
                             ? Constants::NOT_CONNECTED_GRAY
                             : Constants::PLAYER_COLORS[i];
    if (gm.players[i].state == PlayerInfo::NOT_READY)
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
                      Constants::screenHeight - 3*Constants::TEXT_OFFSET},
              Vector2{origin.x / 2, origin.y}, 0, Constants::TEXT_SIZE,
              Constants::TEXT_SPACING, RAYWHITE);
}

void GraphicsManager::DrawLobby(const GameManager &gm)
{
  DrawTitle(gm);
  DrawLobbyPlayers(gm);
  DrawReadyMessage();
  if (gm.new_round_timer > 0) {
    DrawTimer(gm);
  } else {
    DrawExitLobbyMessage();
  }
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
  if (gm.status == GameStatus::END_OF_ROUND)
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

void GraphicsManager::DrawGame(GameManager gameManager) {
  if (gameManager.status == GameStatus::GAME ||
      gameManager.status == GameStatus::END_OF_ROUND) {
    DrawAsteroids(gameManager);
    DrawPlayers(gameManager);
    DrawBullets(gameManager);
    if (gameManager.status == GameStatus::END_OF_ROUND) {
      DrawWinnerText(gameManager);
      DrawNewRoundCountdown(gameManager);
    }
    DrawTime(gameManager, GetTime());
  } else { //LOBBY
    DrawLobby(gameManager);
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
  DrawTextPro(player_font, text,
              Vector2{(float)Constants::screenWidth / 2,
                      Constants::screenHeight / 2 - 2 * Constants::TEXT_OFFSET},
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

void GraphicsManager::DrawRooms(const std::vector<Room> &rooms, int selected) {
  for (int i = 0; i < (int)rooms.size(); i++) {
    std::string room_status =
        (rooms[i].status == GameStatus::GAME) ? "IN GAME" : "LOBBY";
    Color room_color = Constants::NOT_CONNECTED_GRAY;
    float text_size = Constants::TEXT_SIZE;
    char active = ' ';
    if (i == selected) {
      room_color = Constants::PLAYER_COLORS[0];
      text_size *= 1.2;
      active = '>';
    }

    if (rooms[i].players == Constants::PLAYERS_MAX) {
      room_status = "FULL";
      room_color.a = 50;
    }
    float margin = i * 2 * Constants::TEXT_OFFSET;
    const char *text = TextFormat(
        "%c %s[%d/%d] - %s", active, Constants::COOL_ROOM_NAMES[i].c_str(),
        rooms[i].players, Constants::PLAYERS_MAX, room_status.c_str());
    Vector2 origin =
        MeasureTextEx(font, text, text_size, Constants::TEXT_SPACING);

    DrawTextPro(font, text,
                Vector2{(float)Constants::screenWidth / 2,
                        (float)Constants::screenHeight / 2 + margin},
                {origin.x / 2, origin.y / 2}, 0, text_size,
                Constants::TEXT_SPACING, room_color);
  }
}
