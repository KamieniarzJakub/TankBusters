#include "gameManager.hpp"

GameManager::GameManager()
{
    // Utility function from resource_dir.h to find the resources folder and set
    // it as the current working directory so we can load from it
    SearchAndSetResourceDir("resources");

    // Load a texture from the resources directory
    player_texture = LoadTexture("Player.png");
    //font = LoadFont("Roboto-Regular.ttf");
    font = LoadFontEx("Roboto-Regular.ttf", Constants::TEXT_SIZE, nullptr, 0);
    win_font = LoadFontEx("Roboto-Regular.ttf", Constants::TEXT_WIN_SIZE, nullptr, 0);
    player_font = LoadFontEx("Roboto-Regular.ttf", Constants::PLAYER_SIZE, nullptr, 0);
    // Optionally, set texture filtering for smooth font scaling
    //SetTextureFilter(font.texture, TEXTURE_FILTER_ANISOTROPIC_16X);

    NewGame();
}

GameManager::~GameManager()
{
    UnloadTexture(player_texture);
    UnloadFont(font);
    UnloadFont(win_font);
    UnloadFont(player_font);
}

void GameManager::NewGame(int players_in_game)
{
    asteroids = std::vector<Asteroid>(Constants::ASTEROIDS_MAX, Asteroid());
    players = std::vector<Player>(Constants::PLAYERS_MAX, Player());
    bullets = std::vector<Bullet>(Constants::BULLETS_PER_PLAYER * Constants::PLAYERS_MAX, Bullet());
    status = Status::LOBBY;
    _spawnerTime = 0;
    _alive_players = players_in_game;
    startRoundTime = GetTime();
    endRoundTime = -1;
    for (int i = 0; i < Constants::PLAYERS_MAX; i++)
    {
        players[i] = AddPlayer(i, player_font);
    }
}

void GameManager::UpdateGameStatus()
{
    if (_alive_players>1)
    {
        status=Status::GAME;
        endRoundTime = GetTime();
    }
    else if (endRoundTime>0)
    {
        status=Status::END_OF_ROUND;
        if (GetTime()-endRoundTime>=Constants::NEW_ROUND_WAIT_TIME)
        endRoundTime = -1;
    }
    else
    {
        status = Status::LOBBY;
    }
}

void GameManager::UpdatePlayers(float frametime)
{
    for (int i = 0; i < Constants::PLAYERS_MAX; i++)
    {
        UpdatePlayer(&players[i], frametime);
        if (players[i].active && Shoot())
        {
            AddBullet(players[i], i);
        }
    }
}

void GameManager::UpdateBullets(float frametime)
{
    for (int i = 0; i < Constants::PLAYERS_MAX * Constants::BULLETS_PER_PLAYER; i++)
    {
        UpdateBullet(&bullets[i], frametime);
    }
}

void GameManager::UpdateAsteroids(float frametime)
{
    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++)
    {
        UpdateAsteroid(&asteroids[i], frametime);
    }
}

void GameManager::AsteroidSpawner(double time)
{
    if (time > _spawnerTime + Constants::ASTEROID_SPAWN_DELAY)
    {
        TraceLog(LOG_DEBUG, "ASTEROID SPAWNER");
        _spawnerTime = time;
        AddAsteroid();
    }
}

void GameManager::DrawAsteroids()
{
    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++)
    {
        if (!asteroids[i].active) continue;
        DrawAsteroid(asteroids[i]);
    }
}

void GameManager::DrawPlayers()
{
    for (int i = 0; i < Constants::PLAYERS_MAX; i++)
    {
        DrawPlayer(players[i]);
    }
}

void GameManager::DrawBullets()
{
    for (int i = 0; i < Constants::PLAYERS_MAX * Constants::BULLETS_PER_PLAYER; i++)
    {
        if (!bullets[i].active) continue;
        DrawBullet(bullets[i]);
    }
}

void GameManager::DrawTime(double time)
{
    time = status==Status::END_OF_ROUND ? endRoundTime : time; 
    const char* text = TextFormat("%00.2f", time-startRoundTime);
    DrawTextPro(font, text, Vector2{Constants::TEXT_OFFSET, Constants::TEXT_OFFSET}, Vector2{0, 0}, 0.0f, Constants::TEXT_SIZE, Constants::TEXT_SPACING, RAYWHITE);
}

void GameManager::DrawWinnerText()
{
    for (int i=0; i<Constants::PLAYERS_MAX; i++)
    {
        if (players[i].active)
        {
            const char* text = TextFormat("%s PLAYER WIN", Constants::PLAYER_NAMES[i]);
            Vector2 text_length = MeasureTextEx(font, text, Constants::TEXT_WIN_SIZE, Constants::TEXT_SPACING);
            DrawRectangle(0, 0, Constants::screenWidth, Constants::screenHeight, Constants::BACKGROUND_COLOR_HALF_ALFA);
            DrawTextPro(win_font, text, Vector2{Constants::screenWidth/2, Constants::screenHeight/2}, Vector2{text_length.x/2, text_length.y/2}, 0, Constants::TEXT_WIN_SIZE, Constants::TEXT_SPACING, Constants::PLAYER_COLORS[i]);
            return;
        }
    }
}

void GameManager::DrawNewRoundCountdown()
{
    int time = Constants::NEW_ROUND_WAIT_TIME-int(GetTime()-endRoundTime);
    const char* text = TextFormat("New round in %d", time);
    Vector2 origin = MeasureTextEx(font, text, Constants::TEXT_SIZE, Constants::TEXT_SPACING);
    DrawTextPro(font, text, Vector2{Constants::screenWidth/2, Constants::screenHeight}, Vector2{origin.x/2, origin.y+Constants::TEXT_OFFSET}, 0, Constants::TEXT_SIZE, Constants::TEXT_SPACING, RAYWHITE);
}

void GameManager::ManageCollisions()
{
    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++)
    {
        if (!asteroids[i].active) continue;

        for (int j = 0; j < Constants::PLAYERS_MAX; j++)
        {
            for (int k = j * Constants::BULLETS_PER_PLAYER; k < (j + 1) * Constants::BULLETS_PER_PLAYER; k++)
            {
                if (!bullets[k].active) continue;

                if (CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE, asteroids[i].position, asteroids[i].size))
                {
                    bullets[k].active = false;
                    asteroids[i].active = false;
                    SplitAsteroid(asteroids[i].position, asteroids[i].velocity, float(asteroids[i].size) / Constants::ASTEROID_SPLIT_LOSS);
                    break;
                }

                for (int l = 0; l < Constants::PLAYERS_MAX; l++)
                {
                    if (j == l || !players[l].active) continue;

                    if (CheckCollisionCircles(bullets[k].position, Constants::BULLET_SIZE, players[l].position, Constants::PLAYER_SIZE / 3.0f))
                    {
                        players[l].active = false;
                        _alive_players--;
                        bullets[k].active = false;
                        break;
                    }
                }
            }

            if (!players[j].active) continue;

            if (CheckCollisionCircles(players[j].position, Constants::PLAYER_SIZE / 3.0f, asteroids[i].position, asteroids[i].size))
            {
                players[j].active = false;
                _alive_players--;
                asteroids[i].active = false;
            }
        }
    }
    //TraceLog(LOG_DEBUG, "Alive players: %d", _alive_players);
}


void GameManager::AddAsteroid()
{
    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++)
    {
        if (asteroids[i].active) continue;
        asteroids[i] = CreateAsteroid();
        return;
    }
    TraceLog(LOG_ERROR, "Failed to create an asteroid - no empty slots left");
}

void GameManager::SplitAsteroid(Vector2 position, Vector2 velocity, int size)
{
    if (size < Constants::ASTEROID_SIZE_MIN) return;

    int toSpawn = 2;
    for (int i = 0; i < Constants::ASTEROIDS_MAX; i++)
    {
        if (!toSpawn) return;
        if (asteroids[i].active) continue;

        Vector2 new_velocity = Vector2Rotate(velocity, toSpawn % 2 ? GetRandomValue(60, 120) : GetRandomValue(-120, -60));
        asteroids[i] = CreateAsteroid(position, new_velocity, size);
        toSpawn--;
    }

    TraceLog(LOG_ERROR, "Failed to split an asteroid - no empty slots left");
}

void GameManager::AddBullet(Player player, int player_number)
{
    for (int i = player_number * Constants::BULLETS_PER_PLAYER; i < (player_number + 1) * Constants::BULLETS_PER_PLAYER; i++)
    {
        if (bullets[i].active) continue;

        Vector2 offset = Vector2Rotate(Vector2{Constants::PLAYER_SIZE / 2.0f, 0.0f}, player.rotation * DEG2RAD);
        bullets[i] = CreateBullet(Vector2Add(player.position, offset), player.rotation);
        return;
    }

    TraceLog(LOG_INFO, "Failed to shoot a bullet - player[%d]: no bullets left", player_number);
}


