#include "raylib.h"

int main(void) {
    InitWindow(800, 450, "raylib + CLion");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("It works!", 320, 200, 30, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}