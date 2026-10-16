#include "raylib.h"
#include "Ui.h"

// Define a modern slate dark-mode background color (Hex: 0xRRGGBBAA)
#define COLOR_BACKGROUND GetColor(0x1E1E2EFF)

int main(void)
{
    // 1. Configure window flags before creation
    // - FLAG_WINDOW_RESIZABLE: Allows the user to resize the window
    // - FLAG_MSAA_4X_HINT: Smooths out jagged edges on text and shapes
    // - FLAG_VSYNC_HINT: Syncs frame rate with monitor refresh to eliminate screen tearing
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);

    // 2. Initialize the window and core settings
    InitWindow(1100, 700, "Asset Management System");
    SetWindowMinSize(900, 600);
    SetExitKey(KEY_NULL); // Keeps ESC from accidentally closing the app while typing in textboxes
    SetTargetFPS(60);     // Fallback frame rate cap

    // 3. Initialize your UI layer and data structures
    ui_init();

    // 4. Main Application Loop
    while (!WindowShouldClose())
    {
        // --- RENDER PHASE ---
        BeginDrawing();

        // Clear the screen with our sleek custom background color
        ClearBackground(COLOR_BACKGROUND);

        // Update and draw all UI components, buttons, and tables
        ui_update_and_draw();

        EndDrawing();
    }

    // 5. Cleanup and Close
    // (If you write a ui_shutdown() or store_free() function later, call it here)
    CloseWindow();

    return 0;
}