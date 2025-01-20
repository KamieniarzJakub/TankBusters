#include "bullet.hpp"
#include "constants.hpp"
#include "spaceJunkCollector.hpp"
#include "vec2json.hpp"
#include <raymath.h>

Bullet CreateBullet(Vector2 position, float rotation) {
  Bullet bullet;
  bullet.active = true;
  bullet.position = position;
  bullet.rotation = rotation;
  return bullet;
}

void UpdateBullet(Bullet &bullet, duration<double> frametime) {
  if (!bullet.active)
    return;

  if (SpaceJunkCollector(bullet.position)) {
    bullet.active = false;
    return;
  }

  bullet.position.x += Constants::BULLET_SPEED * frametime.count() *
                       cos(bullet.rotation * DEG2RAD);
  bullet.position.y += Constants::BULLET_SPEED * frametime.count() *
                       sin(bullet.rotation * DEG2RAD);
}

void to_json(json &j, const Bullet &b) {
  j = json{
      {"active", b.active}, {"position", b.position}, {"rotation", b.rotation}};
}
void from_json(const json &j, Bullet &b) {
  j.at("active").get_to(b.active);
  j.at("position").get_to(b.position);
  j.at("rotation").get_to(b.rotation);
}
