#include "player.hpp"
#include "colorjson.hpp"
#include "constants.hpp"
#include "vec2json.hpp"

Player AddPlayer(int i) {
  assert(i >= 0 && i <= Constants::PLAYERS_MAX);
  Player player;
  player.active = false;
  player.position = GetPlayerSpawnPosition(i);
  player.velocity = {0, 0}; // Direct initialization of the velocity vector
  player.rotation = (float)GetRandomValue(0, 360);
  player.player_color = Constants::PLAYER_COLORS[i];
  return player;
}

void CalculateUpdatePlayerMovement(Player &player, duration<double> frametime) {

  // Apply damping to velocity
  player.velocity =
      Vector2Scale(player.velocity, (1 - Constants::PLAYER_DRAG / 1000.0f));

  // Update position
  player.position = Vector2Add(
      player.position, Vector2Scale(player.velocity, frametime.count()));

  // Keep player on the screen (wrap around)
  if (player.position.x < 0)
    player.position.x += Constants::screenWidth;
  if (player.position.x > Constants::screenWidth)
    player.position.x -= Constants::screenWidth;
  if (player.position.y < 0)
    player.position.y += Constants::screenHeight;
  if (player.position.y > Constants::screenHeight)
    player.position.y -= Constants::screenHeight;

  // Update player color depending on the active state
  player.player_color.a = player.active ? 255 : 25;
}

void CheckMovementUpdatePlayer(Player &player, duration<double> frametime) {
  int rot = (int)(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) -
            (int)(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
  player.rotation += rot * Constants::PLAYER_ROTATION_SPEED * frametime.count();

  if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
    Vector2 direction = {cosf(DEG2RAD * player.rotation),
                         sinf(DEG2RAD * player.rotation)};
    player.velocity =
        Vector2Add(player.velocity,
                   Vector2Scale(direction, Constants::PLAYER_ACCELERATION *
                                               frametime.count()));
  }
}

bool Shoot() { return IsKeyPressed(KEY_SPACE); }

Vector2 GetPlayerSpawnPosition(int i) {
  assert(i >= 0 && i <= Constants::PLAYERS_MAX);
  return Constants::PLAYER_SPAWN_POSITIONS[i];
}

void to_json(json &j, const Player &p) {

  j = json{{"player_id", p.player_id}, {"active", p.active},
           {"position", p.position},   {"velocity", p.velocity},
           {"rotation", p.rotation},   {"color", p.player_color}};
}
void from_json(const json &j, Player &p) {
  j.at("player_id").get_to(p.player_id);
  j.at("active").get_to(p.active);
  j.at("velocity").get_to(p.velocity);
  j.at("position").get_to(p.position);
  j.at("rotation").get_to(p.rotation);
  j.at("color").get_to(p.player_color);
}
