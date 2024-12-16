#include "raylib.h"
#include "raymath.h"

#include "asteroid.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

int screenWidth = 800;
int screenHeight = 450;
#define BACKGROUND_COLOR CLITERAL(Color){15, 15, 15, 255}

#define ASTEROIDS_MAX 64
static Asteroid _asteroids[ASTEROIDS_MAX] = {0}; 

void UpdateDrawFrame(void);     // Update and Draw one frame

int main()
{
    InitWindow(screenWidth, screenHeight, "Tank Busters");

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);   // Set our game to run at 60 frames-per-second

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdateDrawFrame();
    }
#endif
    CloseWindow();        // Close window and OpenGL context

    return 0;
}

void UpdateDrawFrame(void)
{
    float frametime = GetFrameTime();

    for(int i=0; i<ASTEROIDS_MAX; i++)
    {
        UpdateAsteroid(_asteroids+i, frametime);
    }

    BeginDrawing();

        ClearBackground(BACKGROUND_COLOR);

        for (int i=0; i<ASTEROIDS_MAX; i++)
        {
            DrawAsteroid(_asteroids[i]);
        }
        
    EndDrawing();
}