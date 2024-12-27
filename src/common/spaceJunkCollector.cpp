#include "spaceJunkCollector.hpp"

bool SpaceJunkCollector(Vector2 position)
{
  if (position.x<-Constants::ASTEROID_SIZE_MAX || position.x>Constants::screenWidth+Constants::ASTEROID_SIZE_MAX)
    return true;
  if (position.y<-Constants::ASTEROID_SIZE_MAX || position.y>Constants::screenHeight+Constants::ASTEROID_SIZE_MAX)
    return true;
  return false;
}