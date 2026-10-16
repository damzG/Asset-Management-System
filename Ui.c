//
// Created by User on 10/6/2026.
//
#include "Ui.h"
#include "Store.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ---------- layout ---------- */
#define SIDEBAR_W 220
#define PAD       24
#define ROW_H     40

/* ---------- theme ----------
 * Every colour that changes between light and dark mode lives in this struct.
 * Drawing code reads from `T`; apply_theme() fills it in. */
typedef struct {
    Color bg, card, sidebar, hover, accent, text, muted, rowAlt, rowSel, line, error;
} Theme;

static Theme T;
static bool  darkMode = false;

static const Color STATUS_COLORS[STATUS_COUNT] = {
    {  22, 163,  74, 255 },   /* green  */
    { 100, 116, 139, 255 },   /* grey   */
    { 234,  88,  12, 255 }    /* orange */
};
#define STATUS_NAME(s) store_status_name(s)

static void apply_theme(bool dark)
{
    darkMode = dark;
    if (dark) {
        T = (Theme){
            .bg      = {  15,  23,  42, 255 }, .card   = {  30,  41,  59, 255 },
            .sidebar = {   2,   6,  23, 255 }, .hover  = {  30,  41,  59, 255 },
            .accent  = {  59, 130, 246, 255 }, .text   = { 241, 245, 249, 255 },
            .muted   = { 148, 163, 184, 255 }, .rowAlt = {  36,  48,  68, 255 },
            .rowSel  = {  30,  64, 120, 255 }, .line   = {  51,  65,  85, 255 },
            .error   = { 248, 113, 113, 255 }
        };
    } else {
        T = (Theme){
            .bg      = { 241, 245, 249, 255 }, .card   = { 255, 255, 255, 255 },
            .sidebar = {  30,  41,  59, 255 }, .hover  = {  51,  65,  85, 255 },
            .accent  = {  59, 130, 246, 255 }, .text   = {  15,  23,  42, 255 },
            .muted   = { 100, 116, 139, 255 }, .rowAlt = { 248, 250, 252, 255 },
            .rowSel  = { 219, 234, 254, 255 }, .line   = { 226, 232, 240, 255 },
            .error   = { 220,  38,  38, 255 }
        };
    }

    /* Re-skin raygui widgets (buttons, text boxes) to match */
    #define SET(prop, col) GuiSetStyle(DEFAULT, prop, ColorToInt(col))
    if (dark) {
        SET(BACKGROUND_COLOR,     ((Color){  15,  23,  42, 255 }));
        SET(BORDER_COLOR_NORMAL,  ((Color){  71,  85, 105, 255 }));
        SET(BASE_COLOR_NORMAL,    ((Color){  51,  65,  85, 255 }));
        SET(TEXT_COLOR_NORMAL,    ((Color){ 226, 232, 240, 255 }));
        SET(BORDER_COLOR_FOCUSED, ((Color){  96, 165, 250, 255 }));
        SET(BASE_COLOR_FOCUSED,   ((Color){  71,  85, 105, 255 }));
        SET(TEXT_COLOR_FOCUSED,   ((Color){ 255, 255, 255, 255 }));
        SET(BORDER_COLOR_PRESSED, ((Color){  59, 130, 246, 255 }));
        SET(BASE_COLOR_PRESSED,   ((Color){  30,  64, 120, 255 }));
        SET(TEXT_COLOR_PRESSED,   ((Color){ 255, 255, 255, 255 }));
    } else {   /* raygui's built-in light defaults */
        SET(BACKGROUND_COLOR,     ((Color){ 245, 245, 245, 255 }));
        SET(BORDER_COLOR_NORMAL,  ((Color){ 131, 131, 131, 255 }));
        SET(BASE_COLOR_NORMAL,    ((Color){ 201, 201, 201, 255 }));
        SET(TEXT_COLOR_NORMAL,    ((Color){ 104, 104, 104, 255 }));
        SET(BORDER_COLOR_FOCUSED, ((Color){  91, 178, 217, 255 }));
        SET(BASE_COLOR_FOCUSED,   ((Color){ 201, 239, 254, 255 }));
        SET(TEXT_COLOR_FOCUSED,   ((Color){ 108, 155, 188, 255 }));
        SET(BORDER_COLOR_PRESSED, ((Color){   4, 146, 199, 255 }));
        SET(BASE_COLOR_PRESSED,   ((Color){ 151, 232, 255, 255 }));
        SET(TEXT_COLOR_PRESSED,   ((Color){  54, 139, 175, 255 }));
    }
    #undef SET
}

/* ---------- screens + state ---------- */
typedef enum {
    SCREEN_DASHBOARD,
    SCREEN_ASSETS,
    SCREEN_ADD_ASSET,
    SCREEN_EDIT_ASSET,
    SCREEN_SETTINGS
} Screen;

static Screen currentScreen = SCREEN_DASHBOARD;

/* assets screen */
static char  searchText[64] = "";
static bool  searchEditing  = false;
static float scrollY        = 0.0f;
static int   selectedId     = -1;
static char  notice[96]     = "";      /* e.g. "Could not save to assets.csv" */

/* add / edit form (shared) */
static Asset draft;
static int   editingId      = -1;      /* -1 = adding a new asset, otherwise the id being edited */
static int   activeField    = -1;
static char  formError[64]  = "";

/* settings screen */
static char dbPathBuf[PATH_LEN]     = "";
static char exportPathBuf[PATH_LEN] = "assets_export.csv";
static bool dbEditing = false, exportEditing = false;
static char settingsMsg[128] = "";
static bool settingsMsgIsError = false;

/* ---------- helpers ---------- */
static void reset_draft(void)
{
    memset(&draft, 0, sizeof(draft));
    draft.status = STATUS_IN_STORAGE;
    editingId = -1;
    activeField = -1;
    formError[0] = '\0';
}

/* Saves to the current database file. Call after every add / edit / delete. */
static void persist(void)
{
    if (store_save(store_get_path())) notice[0] = '\0';
    else snprintf(notice, sizeof(notice), "Could not save to %s", store_get_path());
}

static void set_settings_msg(bool isError, const char *fmt, const char *arg)
{
    settingsMsgIsError = isError;
    snprintf(settingsMsg, sizeof(settingsMsg), fmt, arg);
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
    DrawRectangle(0, 0, SIDEBAR_W, GetScreenHeight(), T.sidebar);
    DrawText("Asset Manager", 20, 28, 20, WHITE);

    const struct { const char *label; Screen screen; } items[] = {
        { "Dashboard", SCREEN_DASHBOARD },
        { "Assets",    SCREEN_ASSETS    },
        { "Settings",  SCREEN_SETTINGS  },
    };

    for (int i = 0; i < 3; i++) {
        Rectangle r = { 10, 90.0f + i * 50.0f, SIDEBAR_W - 20, 40 };
        bool onAssets = (currentScreen == SCREEN_ADD_ASSET || currentScreen == SCREEN_EDIT_ASSET);
        bool active = (currentScreen == items[i].screen) ||
                      (onAssets && items[i].screen == SCREEN_ASSETS);
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);

        if (active)     DrawRectangleRounded(r, 0.25f, 8, T.accent);
        else if (hover) DrawRectangleRounded(r, 0.25f, 8, T.hover);

        DrawText(items[i].label, (int)r.x + 16, (int)r.y + 10, 20, WHITE);

        if (hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            currentScreen = items[i].screen;
            activeField = -1;
            searchEditing = false;
            dbEditing = exportEditing = false;
        }
    }

    DrawText("v0.2", 20, GetScreenHeight() - 30, 10, T.muted);
}

/* ---------- dashboard ---------- */
static void draw_dashboard(Rectangle area)
{
    DrawText("Dashboard", (int)area.x, (int)area.y, 30, T.text);

    int counts[STATUS_COUNT] = { 0 };
    for (int i = 0; i < store_count(); i++) counts[store_get(i)->status]++;

    const char *titles[4] = { "Total assets", "In use", "In storage", "In repair" };
    int values[4] = { store_count(), counts[STATUS_IN_USE], counts[STATUS_IN_STORAGE], counts[STATUS_REPAIR] };

    float gap = 20.0f;
    float cardW = (area.width - (3 * gap)) / 4.0f;

    for (int i = 0; i < 4; i++) {
        Rectangle c = { area.x + i * (cardW + gap), area.y + 60, cardW, 110 };
        DrawRectangleRounded(c, 0.1f, 8, T.card);
        DrawText(titles[i], (int)c.x + 20, (int)c.y + 20, 20, T.muted);
        DrawText(TextFormat("%d", values[i]), (int)c.x + 20, (int)c.y + 55, 40, T.accent);
    }

    /* --- Recent Additions Section --- */
    float sectionY = area.y + 195;
    DrawText("Recent Additions", (int)area.x, (int)sectionY, 22, T.text);

    Rectangle recentPanel = { area.x, sectionY + 35, area.width, 210 };
    DrawRectangleRounded(recentPanel, 0.04f, 8, T.card);

    int total = store_count();
    if (total == 0) {
        DrawText("No assets added yet.", (int)recentPanel.x + 20, (int)recentPanel.y + 30, 20, T.muted);
    } else {
        float hY = recentPanel.y + 18;
        DrawText("Name",     (int)recentPanel.x + 20,  (int)hY, 16, T.muted);
        DrawText("Category", (int)recentPanel.x + 280, (int)hY, 16, T.muted);
        DrawText("Status",   (int)recentPanel.x + 500, (int)hY, 16, T.muted);

        hY += 24;
        DrawLine((int)recentPanel.x + 20, (int)hY, (int)(recentPanel.x + recentPanel.width - 20), (int)hY, T.line);

        /* up to 4 most recent additions (looping backwards from the end of the store) */
        int showCount = total < 4 ? total : 4;
        for (int i = 0; i < showCount; i++) {
            Asset *a = store_get(total - 1 - i);
            if (!a) break;

            int rowY = (int)hY + 12 + (i * 34);
            draw_text_fit(a->name, recentPanel.x + 20, (float)rowY, 240, 18, T.text);
            draw_text_fit(a->category, recentPanel.x + 280, (float)rowY, 190, 18, T.muted);
            DrawText(STATUS_NAME(a->status), (int)recentPanel.x + 500, rowY, 18, STATUS_COLORS[a->status]);
        }
    }
}

/* ---------- asset list ---------- */
static void draw_assets(Rectangle area)
{
    DrawText("Assets", (int)area.x, (int)area.y, 30, T.text);
    if (notice[0] != '\0')
        DrawText(notice, (int)area.x + 140, (int)area.y + 8, 18, T.error);

    /* toolbar */
    float toolbarY = area.y + 50;
    Rectangle searchBox = { area.x, toolbarY, 240, 36 };
    Rectangle addBtn    = { area.x + area.width - 350, toolbarY, 140, 36 };
    Rectangle editBtn   = { area.x + area.width - 200, toolbarY,  90, 36 };
    Rectangle delBtn    = { area.x + area.width - 100, toolbarY, 100, 36 };

    if (GuiTextBox(searchBox, searchText, (int)sizeof(searchText), searchEditing))
        searchEditing = !searchEditing;
    if (searchText[0] == '\0' && !searchEditing)
        DrawText("Search...", (int)searchBox.x + 10, (int)searchBox.y + 8, 20, T.muted);

    if (GuiButton(addBtn, "+ Add Asset")) {
        reset_draft();
        currentScreen = SCREEN_ADD_ASSET;
        return;
    }

    bool noSelection = (selectedId < 0);
    if (noSelection) GuiDisable();
    bool editClicked = GuiButton(editBtn, "Edit");
    bool delClicked  = GuiButton(delBtn,  "Delete");
    GuiEnable();

    if (editClicked) {
        Asset *a = store_find_by_id(selectedId);
        if (a) {
            reset_draft();
            draft = *a;               /* copy so Cancel leaves the original untouched */
            editingId = a->id;
            currentScreen = SCREEN_EDIT_ASSET;
            return;
        }
    }
    if (delClicked) {
        store_remove_by_id(selectedId);
        selectedId = -1;
        persist();
    }

    /* table geometry */
    Rectangle table = { area.x, toolbarY + 52, area.width, area.height - (toolbarY + 52 - area.y) };
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
    DrawRectangle((int)table.x, (int)table.y, (int)table.width, ROW_H, T.sidebar);
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

    DrawRectangleRec(body, T.card);

    BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    for (int k = 0; k < n; k++) {
        float y = body.y + k * ROW_H - scrollY;
        if (y + ROW_H < body.y || y > body.y + body.height) continue;

        Asset *a = store_get(rows[k]);
        Rectangle row = { body.x, y, body.width, ROW_H };

        if (a->id == selectedId)  DrawRectangleRec(row, T.rowSel);
        else if (k % 2 == 1)      DrawRectangleRec(row, T.rowAlt);

        if (overBody && CheckCollisionPointRec(mouse, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            selectedId = a->id;

        int ty = (int)y + 10;
        DrawText(TextFormat("%d", a->id), (int)colX[0] + 12, ty, 20, T.muted);
        draw_text_fit(a->name,       colX[1] + 12, (float)ty, colW[1] - 20, 20, T.text);
        draw_text_fit(a->category,   colX[2] + 12, (float)ty, colW[2] - 20, 20, T.text);
        draw_text_fit(a->assignedTo, colX[3] + 12, (float)ty, colW[3] - 20, 20, T.text);
        DrawText(STATUS_NAME(a->status), (int)colX[4] + 12, ty, 20, STATUS_COLORS[a->status]);
    }
    EndScissorMode();

    if (n == 0)
        DrawText("No assets found.", (int)body.x + 16, (int)body.y + 14, 20, T.muted);
}

/* ---------- add / edit form (same screen, two modes) ---------- */
static void draw_asset_form(Rectangle area)
{
    bool editing = (editingId >= 0);
    DrawText(editing ? "Edit Asset" : "Add Asset", (int)area.x, (int)area.y, 30, T.text);

    float cardW = area.width < 600 ? area.width : 600;
    Rectangle card = { area.x, area.y + 60, cardW, 440 };
    DrawRectangleRounded(card, 0.04f, 8, T.card);

    float x = card.x + 24;
    float w = card.width - 48;
    float y = card.y + 24;

    struct { const char *label; char *buf; int size; } fields[] = {
        { "Name",        draft.name,       NAME_LEN  },
        { "Category",    draft.category,   SHORT_LEN },
        { "Assigned to", draft.assignedTo, SHORT_LEN },
    };

    for (int i = 0; i < 3; i++) {
        DrawText(fields[i].label, (int)x, (int)y, 20, T.muted);
        Rectangle box = { x, y + 26, w, 36 };
        if (GuiTextBox(box, fields[i].buf, fields[i].size, activeField == i))
            activeField = (activeField == i) ? -1 : i;
        y += 82;
    }

    DrawText("Status", (int)x, (int)y, 20, T.muted);
    Rectangle statusBtn = { x, y + 26, w, 36 };
    if (GuiButton(statusBtn, TextFormat("%s   (click to change)", STATUS_NAME(draft.status))))
        draft.status = (AssetStatus)((draft.status + 1) % STATUS_COUNT);
    y += 82;

    if (formError[0] != '\0')
        DrawText(formError, (int)x, (int)y - 12, 20, T.error);

    Rectangle saveBtn   = { x, y + 8, 140, 40 };
    Rectangle cancelBtn = { x + 156, y + 8, 140, 40 };

    if (GuiButton(saveBtn, editing ? "Save changes" : "Save")) {
        if (draft.name[0] == '\0') {
            snprintf(formError, sizeof(formError), "Name is required.");
        } else {
            if (editing) {
                store_update(&draft);          /* draft.id is still the original id */
            } else if (store_add(&draft) < 0) {
                snprintf(formError, sizeof(formError), "Asset list is full (%d).", MAX_ASSETS);
                return;
            }
            persist();
            reset_draft();
            currentScreen = SCREEN_ASSETS;
        }
    } else if (GuiButton(cancelBtn, "Cancel")) {
        reset_draft();
        currentScreen = SCREEN_ASSETS;
    }
}

/* ---------- settings ---------- */
static void apply_db_path(void)
{
    if (dbPathBuf[0] == '\0') {
        set_settings_msg(true, "%s", "Enter a file path first.");
        return;
    }

    FILE *f = fopen(dbPathBuf, "r");
    if (f) {                                   /* file exists: switch to it and load */
        fclose(f);
        if (store_load(dbPathBuf)) {
            store_set_path(dbPathBuf);
            selectedId = -1;
            set_settings_msg(false, "Loaded assets from %s", dbPathBuf);
        } else {
            set_settings_msg(true, "Could not read %s", dbPathBuf);
        }
    } else {                                   /* new file: save current assets into it */
        if (store_save(dbPathBuf)) {
            store_set_path(dbPathBuf);
            set_settings_msg(false, "Created %s with current assets", dbPathBuf);
        } else {
            set_settings_msg(true, "Could not create %s", dbPathBuf);
        }
    }
}

static void draw_settings(Rectangle area)
{
    DrawText("Settings", (int)area.x, (int)area.y, 30, T.text);

    float cardW = area.width < 600 ? area.width : 600;
    Rectangle card = { area.x, area.y + 60, cardW, 440 };
    DrawRectangleRounded(card, 0.04f, 8, T.card);

    float x = card.x + 24;
    float w = card.width - 48;
    float y = card.y + 24;

    /* --- theme --- */
    DrawText("Theme", (int)x, (int)y, 20, T.muted);
    if (GuiButton((Rectangle){ x, y + 26, w, 36 },
                  darkMode ? "Dark   (click for light)" : "Light   (click for dark)"))
        apply_theme(!darkMode);
    y += 90;

    /* --- database path --- */
    DrawText("Database file (CSV)", (int)x, (int)y, 20, T.muted);
    if (GuiTextBox((Rectangle){ x, y + 26, w, 36 }, dbPathBuf, (int)sizeof(dbPathBuf), dbEditing))
        dbEditing = !dbEditing;
    if (GuiButton((Rectangle){ x, y + 72, 200, 36 }, "Apply path"))
        apply_db_path();
    y += 130;

    /* --- export --- */
    DrawText("Export report to", (int)x, (int)y, 20, T.muted);
    if (GuiTextBox((Rectangle){ x, y + 26, w, 36 }, exportPathBuf, (int)sizeof(exportPathBuf), exportEditing))
        exportEditing = !exportEditing;
    if (GuiButton((Rectangle){ x, y + 72, 200, 36 }, "Export CSV")) {
        if (exportPathBuf[0] == '\0')
            set_settings_msg(true, "%s", "Enter an export file name first.");
        else if (store_export_csv(exportPathBuf))
            set_settings_msg(false, "Exported to %s", exportPathBuf);
        else
            set_settings_msg(true, "Could not write %s", exportPathBuf);
    }
    y += 130;

    if (settingsMsg[0] != '\0')
        draw_text_fit(settingsMsg, x, y - 10, w, 18, settingsMsgIsError ? T.error : STATUS_COLORS[STATUS_IN_USE]);
}

/* ---------- public API ---------- */
void ui_init(void)
{
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);   /* default font looks sharpest at multiples of 10 */
    apply_theme(false);
    store_init();                           /* loads the CSV, or seeds sample data on first run */
    strncpy(dbPathBuf, store_get_path(), PATH_LEN - 1);
    reset_draft();
}

void ui_update_and_draw(void)
{
    ClearBackground(T.bg);

    Rectangle area = {
        SIDEBAR_W + PAD,
        PAD,
        (float)GetScreenWidth()  - SIDEBAR_W - (2 * PAD),
        (float)GetScreenHeight() - (2 * PAD)
    };

    switch (currentScreen) {
        case SCREEN_DASHBOARD:  draw_dashboard(area);   break;
        case SCREEN_ASSETS:     draw_assets(area);      break;
        case SCREEN_ADD_ASSET:
        case SCREEN_EDIT_ASSET: draw_asset_form(area);  break;
        case SCREEN_SETTINGS:   draw_settings(area);    break;
    }

    draw_sidebar();   /* drawn last so a screen switch never clicks through */
}