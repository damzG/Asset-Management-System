//
// Created by User on 10/6/2026.
//
#include "ui.h"
#include "store.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ---------- layout + colours ---------- */
#define SIDEBAR_W 220
#define PAD       24
#define ROW_H     40

static const Color COL_BG      = { 241, 245, 249, 255 };
static const Color COL_SIDEBAR = {  30,  41,  59, 255 };
static const Color COL_HOVER   = {  51,  65,  85, 255 };
static const Color COL_ACCENT  = {  59, 130, 246, 255 };
static const Color COL_TEXT    = {  15,  23,  42, 255 };
static const Color COL_MUTED   = { 100, 116, 139, 255 };
static const Color COL_ROW_ALT = { 248, 250, 252, 255 };
static const Color COL_ROW_SEL = { 219, 234, 254, 255 };
static const Color COL_ERROR   = { 220,  38,  38, 255 };

static const char *STATUS_NAMES[STATUS_COUNT] = { "In use", "In storage", "In repair" };
static const Color STATUS_COLORS[STATUS_COUNT] = {
    {  22, 163,  74, 255 },   /* green  */
    { 100, 116, 139, 255 },   /* grey   */
    { 234,  88,  12, 255 }    /* orange */
};

/* ---------- screens + state ---------- */
typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_ASSETS,
    SCREEN_ADD_ASSET,
    SCREEN_SETTINGS
} Screen;

static Screen currentScreen = SCREEN_DASHBOARD;

static char  searchText[64] = "";
static bool  searchEditing  = false;
static float scrollY        = 0.0f;
static int   selectedId     = -1;

static Asset draft;                 /* the "Add Asset" form */
static int   activeField    = -1;   /* which form text box has focus */
static char  formError[64]  = "";

/* ---------- helpers ---------- */
static void reset_draft(void)
{
    memset(&draft, 0, sizeof(draft));
    draft.status = STATUS_IN_STORAGE;
    activeField = -1;
    formError[0] = '\0';
}

static bool contains_ci(const char *haystack, const char *needle)
{
    if (needle[0] == '\0') return true;
    size_t n = strlen(needle);
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (i < n && haystack[i] &&
               tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == n) return true;
    }
    return false;
}

static bool matches_search(const Asset *a)
{
    return contains_ci(a->name, searchText) ||
           contains_ci(a->category, searchText) ||
           contains_ci(a->assignedTo, searchText);
}

/* Draws text, shortened with "..." if it would be wider than maxW. */
static void draw_text_fit(const char *text, float x, float y, float maxW, int size, Color color)
{
    char buf[NAME_LEN + 4];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int len = (int)strlen(buf);

    if (MeasureText(buf, size) > maxW) {
        while (len > 0 && MeasureText(TextFormat("%s...", buf), size) > maxW) buf[--len] = '\0';
        DrawText(TextFormat("%s...", buf), (int)x, (int)y, size, color);
    } else {
        DrawText(buf, (int)x, (int)y, size, color);
    }
}

/* ---------- sidebar ---------- */
static void draw_sidebar(void)
{
    DrawRectangle(0, 0, SIDEBAR_W, GetScreenHeight(), COL_SIDEBAR);
    DrawText("Asset Manager", 20, 28, 20, WHITE);

    const struct { const char *label; Screen screen; } items[] = {
        { "Dashboard", SCREEN_DASHBOARD },
        { "Assets",    SCREEN_ASSETS    },
        { "Settings",  SCREEN_SETTINGS  },
    };

    for (int i = 0; i < 3; i++) {
        Rectangle r = { 10, 90.0f + i * 50.0f, SIDEBAR_W - 20, 40 };
        bool active = (currentScreen == items[i].screen) ||
                      (currentScreen == SCREEN_ADD_ASSET && items[i].screen == SCREEN_ASSETS);
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);

        if (active)     DrawRectangleRounded(r, 0.25f, 8, COL_ACCENT);
        else if (hover) DrawRectangleRounded(r, 0.25f, 8, COL_HOVER);

        DrawText(items[i].label, (int)r.x + 16, (int)r.y + 10, 20, WHITE);

        if (hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            currentScreen = items[i].screen;
            activeField = -1;
            searchEditing = false;
        }
    }

    DrawText("v0.1 - UI shell", 20, GetScreenHeight() - 30, 10, COL_MUTED);
}

/* ---------- dashboard ---------- */
static void draw_dashboard(Rectangle area)
{
    DrawText("Dashboard", (int)area.x, (int)area.y, 30, COL_TEXT);

    int counts[STATUS_COUNT] = { 0 };
    for (int i = 0; i < store_count(); i++) counts[store_get(i)->status]++;

    const char *titles[4] = { "Total assets", "In use", "In storage", "In repair" };
    int values[4] = { store_count(), counts[STATUS_IN_USE], counts[STATUS_IN_STORAGE], counts[STATUS_REPAIR] };

    float gap = 20.0f;
    float cardW = (area.width - 3 * gap) / 4.0f;

    for (int i = 0; i < 4; i++) {
        Rectangle c = { area.x + i * (cardW + gap), area.y + 60, cardW, 110 };
        DrawRectangleRounded(c, 0.1f, 8, WHITE);
        DrawText(titles[i], (int)c.x + 20, (int)c.y + 20, 20, COL_MUTED);
        DrawText(TextFormat("%d", values[i]), (int)c.x + 20, (int)c.y + 55, 40, COL_ACCENT);
    }

    DrawText("Charts and recent activity can go here later.", (int)area.x, (int)area.y + 200, 20, COL_MUTED);
}

/* ---------- asset list ---------- */
static void draw_assets(Rectangle area)
{
    DrawText("Assets", (int)area.x, (int)area.y, 30, COL_TEXT);

    /* toolbar */
    float toolbarY = area.y + 50;
    Rectangle searchBox = { area.x, toolbarY, 240, 36 };
    Rectangle addBtn    = { area.x + area.width - 150, toolbarY, 150, 36 };
    Rectangle delBtn    = { addBtn.x - 170, toolbarY, 160, 36 };

    if (GuiTextBox(searchBox, searchText, (int)sizeof(searchText), searchEditing))
        searchEditing = !searchEditing;
    if (searchText[0] == '\0' && !searchEditing)
        DrawText("Search...", (int)searchBox.x + 10, (int)searchBox.y + 8, 20, GRAY);

    if (GuiButton(addBtn, "+ Add Asset")) {
        reset_draft();
        currentScreen = SCREEN_ADD_ASSET;
        return;
    }

    if (selectedId < 0) GuiDisable();
    if (GuiButton(delBtn, "Delete selected")) {
        store_remove_by_id(selectedId);
        selectedId = -1;
    }
    GuiEnable();

    /* table geometry */
    Rectangle table = { area.x, toolbarY + 52, area.width, area.y + area.height - (toolbarY + 52) };
    Rectangle body  = { table.x, table.y + ROW_H, table.width, table.height - ROW_H };

    float colW[5];
    colW[0] = 60;
    colW[1] = table.width * 0.32f;
    colW[2] = table.width * 0.16f;
    colW[3] = table.width * 0.22f;
    colW[4] = table.width - colW[0] - colW[1] - colW[2] - colW[3];
    float colX[5];
    colX[0] = table.x;
    for (int i = 1; i < 5; i++) colX[i] = colX[i - 1] + colW[i - 1];
    const char *headers[5] = { "ID", "Name", "Category", "Assigned to", "Status" };

    /* header */
    DrawRectangle((int)table.x, (int)table.y, (int)table.width, ROW_H, COL_SIDEBAR);
    for (int i = 0; i < 5; i++)
        DrawText(headers[i], (int)colX[i] + 12, (int)table.y + 10, 20, WHITE);

    /* rows that match the search */
    int rows[MAX_ASSETS];
    int n = 0;
    for (int i = 0; i < store_count(); i++)
        if (matches_search(store_get(i))) rows[n++] = i;

    /* scrolling */
    Vector2 mouse = GetMousePosition();
    bool overBody = CheckCollisionPointRec(mouse, body);
    float maxScroll = (float)n * ROW_H - body.height;
    if (maxScroll < 0) maxScroll = 0;
    if (overBody) scrollY -= GetMouseWheelMove() * ROW_H;
    if (scrollY > maxScroll) scrollY = maxScroll;
    if (scrollY < 0) scrollY = 0;

    DrawRectangleRec(body, WHITE);

    BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    for (int k = 0; k < n; k++) {
        float y = body.y + k * ROW_H - scrollY;
        if (y + ROW_H < body.y || y > body.y + body.height) continue;

        Asset *a = store_get(rows[k]);
        Rectangle row = { body.x, y, body.width, ROW_H };

        if (a->id == selectedId)  DrawRectangleRec(row, COL_ROW_SEL);
        else if (k % 2 == 1)      DrawRectangleRec(row, COL_ROW_ALT);

        if (overBody && CheckCollisionPointRec(mouse, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            selectedId = a->id;

        int ty = (int)y + 10;
        DrawText(TextFormat("%d", a->id), (int)colX[0] + 12, ty, 20, COL_MUTED);
        draw_text_fit(a->name,       colX[1] + 12, (float)ty, colW[1] - 20, 20, COL_TEXT);
        draw_text_fit(a->category,   colX[2] + 12, (float)ty, colW[2] - 20, 20, COL_TEXT);
        draw_text_fit(a->assignedTo, colX[3] + 12, (float)ty, colW[3] - 20, 20, COL_TEXT);
        DrawText(STATUS_NAMES[a->status], (int)colX[4] + 12, ty, 20, STATUS_COLORS[a->status]);
    }
    EndScissorMode();

    if (n == 0)
        DrawText("No assets found.", (int)body.x + 16, (int)body.y + 14, 20, COL_MUTED);
}

/* ---------- add-asset form ---------- */
static void draw_add_asset(Rectangle area)
{
    DrawText("Add Asset", (int)area.x, (int)area.y, 30, COL_TEXT);

    float cardW = area.width < 600 ? area.width : 600;
    Rectangle card = { area.x, area.y + 60, cardW, 440 };
    DrawRectangleRounded(card, 0.04f, 8, WHITE);

    float x = card.x + 24;
    float w = card.width - 48;
    float y = card.y + 24;

    struct { const char *label; char *buf; int size; } fields[] = {
        { "Name",        draft.name,       NAME_LEN  },
        { "Category",    draft.category,   SHORT_LEN },
        { "Assigned to", draft.assignedTo, SHORT_LEN },
    };

    for (int i = 0; i < 3; i++) {
        DrawText(fields[i].label, (int)x, (int)y, 20, COL_MUTED);
        Rectangle box = { x, y + 26, w, 36 };
        if (GuiTextBox(box, fields[i].buf, fields[i].size, activeField == i))
            activeField = (activeField == i) ? -1 : i;
        y += 82;
    }

    DrawText("Status", (int)x, (int)y, 20, COL_MUTED);
    Rectangle statusBtn = { x, y + 26, w, 36 };
    if (GuiButton(statusBtn, TextFormat("%s   (click to change)", STATUS_NAMES[draft.status])))
        draft.status = (AssetStatus)((draft.status + 1) % STATUS_COUNT);
    y += 82;

    if (formError[0] != '\0')
        DrawText(formError, (int)x, (int)y - 12, 20, COL_ERROR);

    Rectangle saveBtn   = { x, y + 8, 140, 40 };
    Rectangle cancelBtn = { x + 156, y + 8, 140, 40 };

    if (GuiButton(saveBtn, "Save")) {
        if (draft.name[0] == '\0') {
            snprintf(formError, sizeof(formError), "Name is required.");
        } else {
            store_add(&draft);
            reset_draft();
            currentScreen = SCREEN_ASSETS;
        }
    } else if (GuiButton(cancelBtn, "Cancel")) {
        reset_draft();
        currentScreen = SCREEN_ASSETS;
    }
}

/* ---------- settings ---------- */
static void draw_settings(Rectangle area)
{
    DrawText("Settings", (int)area.x, (int)area.y, 30, COL_TEXT);
    DrawText("Nothing here yet. Theme, database path and export options can live here.",
             (int)area.x, (int)area.y + 60, 20, COL_MUTED);
}

/* ---------- public API ---------- */
void ui_init(void)
{
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);   /* default font looks sharpest at multiples of 10 */
    store_init_placeholder();
    reset_draft();
}

void ui_update_and_draw(void)
{
    ClearBackground(COL_BG);

    Rectangle area = {
        SIDEBAR_W + PAD, PAD,
        (float)GetScreenWidth()  - SIDEBAR_W - 2 * PAD,
        (float)GetScreenHeight() - 2 * PAD
    };

    switch (currentScreen) {
        case SCREEN_DASHBOARD: draw_dashboard(area); break;
        case SCREEN_ASSETS:    draw_assets(area);    break;
        case SCREEN_ADD_ASSET: draw_add_asset(area); break;
        case SCREEN_SETTINGS:  draw_settings(area);  break;
    }

    draw_sidebar();   /* drawn last so a screen switch never clicks through */
}