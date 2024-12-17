#pragma once
#include "raylib.h"
#include <string>

namespace Constants {
const static int screenWidth = 1600;
const static int screenHeight = 900;
const static std::string windowTitle = "Tank Busters!";
const static int ASTEROID_ROTATION_SPEED_MIN = 100;
const static int ASTEROID_ROTATION_SPEED_MAX = 250;
const static Color BACKGROUND_COLOR = {15, 15, 15, 255};
const static int ASTEROID_MAX = 64;
} // namespace Constants
