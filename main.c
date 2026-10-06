#include "raylib.h"
#include "ui.h"

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1100, 700, "Asset Management System");
    SetWindowMinSize(900, 600);
    SetExitKey(KEY_NULL);          /* Esc must not close the app while typing */
    SetTargetFPS(60);

    ui_init();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ui_update_and_draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}