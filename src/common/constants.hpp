#pragma once
#include "raylib.h"
#include <chrono>
#include <string>
using namespace std::chrono;

namespace Constants {
const static int screenWidth = 900;
const static int screenHeight = 650;
const static std::string windowTitle = "Tank Busters!";

const static std::string COOL_ROOM_NAMES[] = {"BLUE SHARK", "STONEBREAKER",
                                              "POTATO KING", "FOOTBALL FAN"};
const static int ASTEROID_SPEED_MIN = 100;
const static int ASTEROID_SPEED_MAX = 200;
const static int ASTEROID_ROTATION_SPEED_MAX = 200;
const static int ASTEROID_SIZE_MIN = 16;
const static int ASTEROID_SIZE_MAX = 64;
const static int ASTEROID_POLYGON_MIN = 5;
const static int ASTEROID_POLYGON_MAX = 8;
const static int ASTEROID_PATH_RANDOM_ANGLE = 45 * DEG2RAD;
const static int ASTEROIDS_MAX = 64;
const static auto ASTEROID_SPAWN_DELAY = 1s;
const static float ASTEROID_SPLIT_LOSS = 1.5f;

const static int PLAYERS_MAX = 4;
const static float PLAYER_ROTATION_SPEED = 150.0f;
const static float PLAYER_ACCELERATION = 650.0f;
const static float PLAYER_DRAG = 3.0f;
const static int PLAYER_SIZE = 100.0f;
const static std::string PLAYER_AVATAR = "A";
const static std::string PLAYER_NAMES[4] = {"BLUE", "RED", "GREEN", "YELLOW"};
const static Color PLAYER_COLORS[4] = {{66, 133, 244, 255},
                                       {255, 0, 51, 255},
                                       {52, 168, 83, 255},
                                       {251, 188, 4, 255}};
const static Vector2 PLAYER_SPAWN_POSITIONS[4] = {
    Vector2{(float)screenWidth / 2 - 50, (float)screenHeight / 2 - 50},
    Vector2{(float)screenWidth / 2 + 50, (float)screenHeight / 2 - 50},
    Vector2{(float)screenWidth / 2 - 50, (float)screenHeight / 2 + 50},
    Vector2{(float)screenWidth / 2 + 50, (float)screenHeight / 2 + 50}};
const static int BULLETS_PER_PLAYER = 3;
const static float BULLET_SPEED = 350.0f;
const static float BULLET_SIZE = 5.0f;

const static std::string ROBOTO_REGULAR = "Roboto-Regular.ttf";
const static int TEXT_OFFSET = 30;
const static float TEXT_SIZE = 50.0f;
const static float TEXT_WIN_SIZE = 100.0f;
const static float TEXT_SPACING = 1.0f;

const static Color BACKGROUND_COLOR = {15, 15, 15, 255};
const static Color BACKGROUND_COLOR_HALF_ALFA = {15, 15, 15, 150};
const static Color NOT_CONNECTED_GRAY = {200, 200, 200, 150};

const static auto NEW_ROUND_WAIT_TIME = 3s;
const static auto LOBBY_READY_TIME = 5s;
const static unsigned int ROUNDS_PER_GAME = 3;

const static unsigned int CONNECTION_TIMEOUT_MILISECONDS = 5000;
const static std::chrono::milliseconds ROOM_FETCH_INTERVAL{5000};
} // namespace Constants
