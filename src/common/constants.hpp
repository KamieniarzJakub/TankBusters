#pragma once
#include "raylib.h"
#include <string>

namespace Constants {
const static int screenWidth = 600;
const static int screenHeight = 450;
const static std::string windowTitle = "Tank Busters!";

const static int ASTEROID_SPEED_MIN = 100;
const static int ASTEROID_SPEED_MAX = 200;
const static int ASTEROID_ROTATION_SPEED_MIN = 100;
const static int ASTEROID_ROTATION_SPEED_MAX = 200;
const static int ASTEROID_SIZE_MIN = 25;
const static int ASTEROID_SIZE_MAX = 64;
const static int ASTEROID_POLYGON_MIN = 5;
const static int ASTEROID_POLYGON_MAX = 8;
const static int ASTEROID_PATH_RANDOM_ANGLE = 45 * DEG2RAD;
const static int ASTEROIDS_MAX = 64;
const static float ASTEROID_SPAWN_DELAY = 1.0f;

const static int PLAYERS_MAX = 4;

const static Color BACKGROUND_COLOR = {15, 15, 15, 255};
} // namespace Constants
