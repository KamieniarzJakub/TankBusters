#pragma once

#include "asteroid.hpp"
#include "constants.hpp"
#include "player.hpp"
#include "bullet.hpp"
#include "raylib.h"
#include "raymath.h"
#include "vector"
#include "resource_dir.hpp"

enum Status
{
    LOBBY = 0,
    GAME = 1,
    END_OF_ROUND = 2
};

struct GameManager
{
    std::vector<Asteroid> asteroids;
    std::vector<Player> players;
    std::vector<Bullet> bullets; 

    int status;
    int _alive_players;
    float _spawnerTime;
    Texture player_texture;
    Font font;

    GameManager();
    ~GameManager();

    void NewGame();

    void UpdatePlayers(float frametime);
    void UpdateBullets(float frametime);
    void UpdateAsteroids(float frametime);

    void AsteroidSpawner(double time);

    void DrawAsteroids();
    void DrawPlayers();
    void DrawBullets();
    void DrawTime(double time);

    void ManageCollisions();

    void AddAsteroid();
    void SplitAsteroid(Vector2 position, Vector2 velocity, int size);
    void AddBullet(Player player, int player_number);

    int GetGameStatus();
};
