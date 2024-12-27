#include "bullet.hpp"


Bullet CreateBullet(Vector2 position, float rotation)
{
    Bullet bullet;
    bullet.active = true;
    bullet.position = position;
    bullet.rotation = rotation;
    return bullet;
}

void UpdateBullet(Bullet *bullet, float frametime) {
  if (!bullet->active)
    return;

  if (SpaceJunkCollector(bullet->position))
  {
    bullet->active = false;
    return;
  }

  bullet->position.x += Constants::BULLET_SPEED * frametime * cos(bullet->rotation * DEG2RAD);
  bullet->position.y += Constants::BULLET_SPEED * frametime * sin(bullet->rotation * DEG2RAD);
}

void DrawBullet(Bullet bullet)
{
    DrawCircle(bullet.position.x, bullet.position.y, Constants::BULLET_SIZE, WHITE);
}