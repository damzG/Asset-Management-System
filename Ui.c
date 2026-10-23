//
// Created by User on 10/6/2026.
//
#include "Ui.h"
#include "Store.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include "rlgl.h"          /* raw quad drawing for the shaded 3D boxes */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------- layout ---------- */
#define SIDEBAR_W 220
#define PAD       24
#define ROW_H     40

/* ---------- theme ----------
 * Every colour that changes between light and dark mode lives in this struct.
 * Drawing code reads from `T`; apply_theme() fills it in. */
typedef struct {
    Color bg, card, sidebar, hover, accent, text, muted, rowAlt, rowSel, line, error, warn;
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
            .error   = { 248, 113, 113, 255 }, .warn = { 251, 191,  36, 255 }
        };
    } else {
        T = (Theme){
            .bg      = { 241, 245, 249, 255 }, .card   = { 255, 255, 255, 255 },
            .sidebar = {  30,  41,  59, 255 }, .hover  = {  51,  65,  85, 255 },
            .accent  = {  59, 130, 246, 255 }, .text   = {  15,  23,  42, 255 },
            .muted   = { 100, 116, 139, 255 }, .rowAlt = { 248, 250, 252, 255 },
            .rowSel  = { 219, 234, 254, 255 }, .line   = { 226, 232, 240, 255 },
            .error   = { 220,  38,  38, 255 }, .warn = { 217, 119,   6, 255 }
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
static bool  confirmDelete  = false;   /* "are you sure?" dialog is open */
static int   confirmId       = -1;      /* asset the dialog is asking about */
static char  notice[96]     = "";      /* e.g. "Could not save to assets.csv" */

/* add / edit form (shared) */
static Asset draft;
static int   editingId      = -1;      /* -1 = adding a new asset, otherwise the id being edited */
static int   activeField    = -1;
static char  formError[64]  = "";
static char  deadlineBuf[16] = "";     /* "YYYY-MM-DD" text box in the form */

/* deadlines */
static time_t nowT = 0;                /* "current time" the whole UI uses (may be simulated) */

/* time-travel clock */
static const double SPEEDS[] = { -86400, -3600, -60, -10, -1, 0, 1, 10, 60, 3600, 86400 };  /* sim seconds per real second */
#define SPEED_COUNT  ((int)(sizeof(SPEEDS) / sizeof(SPEEDS[0])))
#define SPEED_PAUSED 5
#define SPEED_REAL   6
static double simT     = 0;            /* simulated epoch seconds */
static int    speedIdx = SPEED_REAL;
static int    alertDismissedAt = 0;    /* toast hidden until the overdue count rises above this */

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
    deadlineBuf[0] = '\0';
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

/* ---------- deadline assistant ----------
 * Rule-based "assistant": finds overdue / due-soon assets, ranks them by how urgent
 * they are, and suggests a next step based on status and who holds the asset. */
static Color urgency_color(Urgency u)
{
    switch (u) {
        case URGENCY_OVERDUE: return T.error;
        case URGENCY_SOON:    return T.warn;
        case URGENCY_OK:      return STATUS_COLORS[STATUS_IN_USE];
        default:              return T.muted;
    }
}

static int cmp_deadline(const void *a, const void *b)
{
    time_t da = store_get(*(const int *)a)->deadline;
    time_t db = store_get(*(const int *)b)->deadline;
    return (da > db) - (da < db);          /* earliest (most overdue) first */
}

/* Fills idx[] with overdue + due-soon assets, most urgent first. Returns how many. */
static int collect_urgent(int idx[], int *overdue, int *soon)
{
    int n = 0;
    *overdue = *soon = 0;
    for (int i = 0; i < store_count(); i++) {
        Urgency u = store_urgency(store_get(i), nowT);
        if (u == URGENCY_OVERDUE)   { (*overdue)++; idx[n++] = i; }
        else if (u == URGENCY_SOON) { (*soon)++;    idx[n++] = i; }
    }
    qsort(idx, (size_t)n, sizeof(int), cmp_deadline);
    return n;
}

static void assistant_hint(const Asset *a, Urgency u, char *buf, size_t n)
{
    bool hasPerson = (a->assignedTo[0] != '\0');
    if (u == URGENCY_OVERDUE) {
        if (a->status == STATUS_REPAIR)           snprintf(buf, n, "still in repair - chase the repairer");
        else if (a->status == STATUS_IN_USE && hasPerson)
                                                  snprintf(buf, n, "ask %s to return it or extend", a->assignedTo);
        else                                      snprintf(buf, n, "review it and set a new deadline");
    } else {
        if (a->status == STATUS_REPAIR)           snprintf(buf, n, "make sure the repair finishes in time");
        else if (hasPerson)                       snprintf(buf, n, "remind %s", a->assignedTo);
        else                                      snprintf(buf, n, "plan ahead");
    }
}

/* Small red pill in the top-right corner while assets are overdue. Click to dismiss. */
static void draw_overdue_toast(Rectangle area)
{
    int idx[MAX_ASSETS], overdue, soon;
    collect_urgent(idx, &overdue, &soon);
    if (overdue < alertDismissedAt) alertDismissedAt = overdue;   /* re-arm if the count drops */
    if (overdue <= alertDismissedAt) return;

    const char *msg = TextFormat("%d overdue asset%s  (click to dismiss)", overdue, overdue == 1 ? "" : "s");
    int tw = MeasureText(msg, 18);
    Rectangle r = { area.x + area.width - (tw + 40), 8, (float)tw + 40, 34 };

    DrawRectangleRounded(r, 0.4f, 8, T.error);
    DrawCircle((int)r.x + 18, (int)(r.y + 17), 8, WHITE);
    DrawText("!", (int)r.x + 15, (int)r.y + 9, 18, T.error);
    DrawText(msg, (int)r.x + 32, (int)r.y + 8, 18, WHITE);

    if (!confirmDelete && CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        alertDismissedAt = overdue;
}

/* ---------- time travel (testing aid) ----------
 * The app reads "now" from a simulated clock (simT). It normally runs at real time,
 * but you can pause it, speed it up, run it backwards, or jump by hours / days to
 * watch countdowns, warnings and the overdue toast react. Nothing is saved. */
static void draw_time_travel(void)
{
    float x = 10, w = SIDEBAR_W - 20;
    float top = (float)GetScreenHeight() - 250;
    DrawRectangleRounded((Rectangle){ x, top - 8, w, 214 }, 0.06f, 8, Fade(WHITE, 0.07f));

    double drift = simT - (double)time(NULL);
    bool simulated = (speedIdx != SPEED_REAL) || drift > 2.0 || drift < -2.0;

    DrawText("Time travel", (int)x + 8, (int)top, 16, T.muted);
    if (simulated) DrawText("SIMULATED", (int)(x + w - 8 - MeasureText("SIMULATED", 14)), (int)top + 2, 14, T.warn);

    char stamp[32] = "";
    time_t t = nowT;
    struct tm *lt = localtime(&t);
    if (lt) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", lt);
    DrawText(stamp, (int)x + 8, (int)top + 22, 16, WHITE);

    double sp = SPEEDS[speedIdx];
    const char *speedTxt = (sp == 0) ? "Paused"
                         : (speedIdx == SPEED_REAL) ? "Real time (1x)"
                         : TextFormat("Speed: %.0fx%s", sp, sp < 0 ? " (reverse)" : "");
    DrawText(speedTxt, (int)x + 8, (int)top + 44, 16, simulated ? T.warn : T.muted);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

    float bw3 = (w - 12) / 3.0f, bw2 = (w - 6) / 2.0f;
    float by = top + 68;

    if (speedIdx == 0) GuiDisable();
    if (GuiButton((Rectangle){ x, by, bw3, 30 }, "<<")) speedIdx--;
    GuiEnable();

    if (GuiButton((Rectangle){ x + bw3 + 6, by, bw3, 30 }, speedIdx == SPEED_PAUSED ? ">" : "||"))
        speedIdx = (speedIdx == SPEED_PAUSED) ? SPEED_REAL : SPEED_PAUSED;

    if (speedIdx == SPEED_COUNT - 1) GuiDisable();
    if (GuiButton((Rectangle){ x + 2 * (bw3 + 6), by, bw3, 30 }, ">>")) speedIdx++;
    GuiEnable();

    by = top + 104;
    if (GuiButton((Rectangle){ x, by, bw2, 30 }, "-1 day"))  simT -= 86400.0;
    if (GuiButton((Rectangle){ x + bw2 + 6, by, bw2, 30 }, "+1 day")) simT += 86400.0;

    by = top + 140;
    if (GuiButton((Rectangle){ x, by, bw2, 30 }, "-1 hour"))  simT -= 3600.0;
    if (GuiButton((Rectangle){ x + bw2 + 6, by, bw2, 30 }, "+1 hour")) simT += 3600.0;

    by = top + 176;
    if (GuiButton((Rectangle){ x, by, w, 30 }, "Reset to real time")) {
        simT = (double)time(NULL);
        speedIdx = SPEED_REAL;
    }

    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
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
        bool hover = !confirmDelete && CheckCollisionPointRec(GetMousePosition(), r);

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

    draw_time_travel();
    DrawText("v0.4", 20, GetScreenHeight() - 30, 10, T.muted);
}

/* ---------- dashboard ---------- */
static void draw_dashboard(Rectangle area)
{
    DrawText("Dashboard", (int)area.x, (int)area.y, 30, T.text);

    int counts[STATUS_COUNT] = { 0 };
    for (int i = 0; i < store_count(); i++) counts[store_get(i)->status]++;

    int urgent[MAX_ASSETS], overdue, soon;
    int nUrgent = collect_urgent(urgent, &overdue, &soon);

    const char *titles[5] = { "Total assets", "In use", "In storage", "In repair", "Overdue" };
    int values[5] = { store_count(), counts[STATUS_IN_USE], counts[STATUS_IN_STORAGE], counts[STATUS_REPAIR], overdue };

    float gap = 16.0f;
    float cardW = (area.width - (4 * gap)) / 5.0f;

    for (int i = 0; i < 5; i++) {
        Rectangle c = { area.x + i * (cardW + gap), area.y + 60, cardW, 110 };
        DrawRectangleRounded(c, 0.1f, 8, T.card);
        DrawText(titles[i], (int)c.x + 20, (int)c.y + 20, 20, T.muted);
        Color numCol = (i == 4 && overdue > 0) ? T.error : T.accent;
        DrawText(TextFormat("%d", values[i]), (int)c.x + 20, (int)c.y + 55, 40, numCol);
    }

    /* --- Deadline Assistant --- */
    int withDeadline = 0;
    for (int i = 0; i < store_count(); i++) if (store_get(i)->deadline) withDeadline++;

    Rectangle banner = { area.x, area.y + 184, area.width, 114 };
    DrawRectangleRounded(banner, 0.08f, 8, T.card);

    Color tone = overdue > 0 ? T.error : (soon > 0 ? T.warn : STATUS_COLORS[STATUS_IN_USE]);
    DrawRectangle((int)banner.x, (int)banner.y + 8, 5, (int)banner.height - 16, tone);

    float bx = banner.x + 22, bw = banner.width - 44;
    DrawText("Deadline Assistant", (int)bx, (int)banner.y + 10, 16, T.muted);

    if (withDeadline == 0) {
        DrawText("No deadlines set yet. Edit an asset and add one to get reminders.",
                 (int)bx, (int)banner.y + 34, 18, T.text);
    } else if (nUrgent == 0) {
        DrawText(TextFormat("All clear - nothing overdue or due within %d days.", DEADLINE_SOON_DAYS),
                 (int)bx, (int)banner.y + 34, 20, tone);
    } else {
        draw_text_fit(TextFormat("%d overdue, %d due within %d days", overdue, soon, DEADLINE_SOON_DAYS),
                      bx, banner.y + 30, bw, 20, tone);

        int shown = nUrgent < 3 ? nUrgent : 3;
        for (int i = 0; i < shown; i++) {
            Asset *a = store_get(urgent[i]);
            Urgency u = store_urgency(a, nowT);
            char cd[48], hint[96], line[256];
            store_countdown_text(a, nowT, cd, sizeof(cd));
            assistant_hint(a, u, hint, sizeof(hint));
            snprintf(line, sizeof(line), "%s: %s - %s", a->name, cd, hint);
            DrawCircle((int)bx + 4, (int)banner.y + 66 + i * 18, 4, urgency_color(u));
            draw_text_fit(line, bx + 16, banner.y + 58 + i * 18, bw - 16, 16, T.text);
        }
        if (nUrgent > shown)
            DrawText(TextFormat("+%d more", nUrgent - shown), (int)(banner.x + banner.width - 90),
                     (int)banner.y + 10, 16, T.muted);
    }

    /* --- Recent Additions Section --- */
    float sectionY = area.y + 312;
    DrawText("Recent Additions", (int)area.x, (int)sectionY, 22, T.text);

    Rectangle recentPanel = { area.x, sectionY + 35, area.width, 195 };
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

/* ---------- 3D warehouse view ----------
 * Assets are small boxes on the shelves of a warehouse, coloured by status. Overdue /
 * due-soon assets get a red / amber marker floating above them.
 *   - Adding an asset: a delivery truck backs up to the dock and a worker carries the box
 *     in and puts it on its shelf.
 *   - Deleting an asset: a worker walks in, takes the box off the shelf and carries it out.
 * Drag to orbit, scroll to zoom, click a box to select it (Edit / Delete then work on it). */

/* --- warehouse dimensions (world units) --- */
#define BOX_SIZE   0.75f
#define BAY_W      1.05f                 /* width of one shelf bay */
#define BAYS_PER_RACK 8                  /* bays along one rack */
#define RACK_LEN   (BAYS_PER_RACK * BAY_W)
#define RACK_D     1.1f
#define LEVEL_H    1.3f                  /* 3 shelf levels */
#define PLANK_T    0.08f
#define RACK_H     (3 * LEVEL_H + 0.1f)
#define AISLE_W    2.4f                  /* central aisle between the left and right racks */
#define ROW_GAP    2.8f                  /* aisle between rows of racks */
#define MARGIN     1.0f
#define ROW0_Z     1.3f
#define RACK_ROWS  6
#define PER_RACK   (3 * BAYS_PER_RACK)
#define SLOT_COUNT (RACK_ROWS * 2 * PER_RACK)
#define WH_W       (2 * MARGIN + 2 * RACK_LEN + AISLE_W)
#define WH_D       (ROW0_Z + RACK_ROWS * RACK_D + (RACK_ROWS - 1) * ROW_GAP + ROW_GAP)
#define DOOR_X     (WH_W / 2)            /* loading door in the front wall (z = WH_D) */
#define DOOR_W     3.4f
#define DOOR_H     3.4f
#define WALL_H     4.8f
#define WALL_T     0.3f

/* --- animation timing --- */
#define WALK_SPEED 4.0f
#define T_ARRIVE   2.4f                  /* truck backs up to the dock */
#define T_UNLOAD   0.6f
#define T_DEPART   2.4f
#define T_PLACE    0.6f                  /* putting a box on the shelf */
#define T_PICK     0.6f                  /* taking a box off the shelf */
#define T_FADE     0.5f
#define TRUCK_STOP (WH_D + 0.9f)         /* z of the truck's rear when parked at the dock */
#define TRUCK_FAR  (WH_D + 24.0f)         /* starts out on the road */
#define TRUCK_CLEAR (T_ARRIVE + T_UNLOAD + T_DEPART)

static bool  view3D   = false;
static float camYaw   = 0.6f, camPitch = 0.65f, camDist = 0.0f;   /* camDist 0 = default distance */
static bool  orbiting = false;
static float dragDist = 0.0f;
static RenderTexture2D sceneRT;        /* the 3D scene is rendered here, then drawn into the panel */
static int   sceneW = 0, sceneH = 0;
static bool  fullView3D = false;        /* 3D view covers the whole window (no sidebar / toolbar) */

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float s) { s = s < 0 ? 0 : (s > 1 ? 1 : s); return s * s * (3 - 2 * s); }

static Color shade(Color c, float f)
{
    float r = c.r * f, g = c.g * f, b = c.b * f;
    return (Color){ (unsigned char)(r > 255 ? 255 : r), (unsigned char)(g > 255 ? 255 : g),
                    (unsigned char)(b > 255 ? 255 : b), c.a };
}

static Color mix(Color a, Color b, float t)
{
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

/* A box with a different brightness per side, so it reads as 3D without any lighting setup. */
static void draw_box(Vector3 c, float sx, float sy, float sz, Color col)
{
    float x0 = c.x - sx / 2, x1 = c.x + sx / 2;
    float y0 = c.y - sy / 2, y1 = c.y + sy / 2;
    float z0 = c.z - sz / 2, z1 = c.z + sz / 2;
    Color top = shade(col, 1.00f), front = shade(col, 0.80f), side = shade(col, 0.62f);

    rlBegin(RL_QUADS);
    rlColor4ub(top.r, top.g, top.b, top.a);                      /* top   */
    rlVertex3f(x0, y1, z0); rlVertex3f(x0, y1, z1); rlVertex3f(x1, y1, z1); rlVertex3f(x1, y1, z0);
    rlColor4ub(front.r, front.g, front.b, front.a);              /* front + back */
    rlVertex3f(x0, y0, z1); rlVertex3f(x1, y0, z1); rlVertex3f(x1, y1, z1); rlVertex3f(x0, y1, z1);
    rlVertex3f(x1, y0, z0); rlVertex3f(x0, y0, z0); rlVertex3f(x0, y1, z0); rlVertex3f(x1, y1, z0);
    rlColor4ub(side.r, side.g, side.b, side.a);                  /* left + right */
    rlVertex3f(x1, y0, z1); rlVertex3f(x1, y0, z0); rlVertex3f(x1, y1, z0); rlVertex3f(x1, y1, z1);
    rlVertex3f(x0, y0, z0); rlVertex3f(x0, y0, z1); rlVertex3f(x0, y1, z1); rlVertex3f(x0, y1, z0);
    rlEnd();
}

/* ---- shelf slots ----
 * Every asset lives in a fixed shelf slot, so boxes never shuffle around when something is
 * added or deleted. Slots are handed out in order: rack by rack, bottom shelf first. */
typedef struct { Vector3 pos; float pickX, pickZ; } SlotInfo;   /* pos = bottom centre of the box */

static SlotInfo slot_info(int slot)
{
    int rack = slot / PER_RACK, inRack = slot % PER_RACK;
    int row = rack / 2, side = rack % 2;
    int level = inRack / BAYS_PER_RACK, bay = inRack % BAYS_PER_RACK;
    float x0 = MARGIN + side * (RACK_LEN + AISLE_W);
    float z0 = ROW0_Z + row * (RACK_D + ROW_GAP);

    SlotInfo si;
    si.pos   = (Vector3){ x0 + (bay + 0.5f) * BAY_W, PLANK_T + level * LEVEL_H, z0 + RACK_D / 2 };
    si.pickX = si.pos.x;
    si.pickZ = z0 + RACK_D + 0.9f;       /* where a worker stands, in the aisle in front of the rack */
    return si;
}

static int slotId[SLOT_COUNT];           /* asset id stored in each slot (0 = empty) */

static int slot_of(int id)
{
    for (int s = 0; s < SLOT_COUNT; s++) if (slotId[s] == id) return s;
    return -1;
}

/* ---- delivery / removal animations ---- */
typedef enum { JOB_DELIVER, JOB_REMOVE } JobKind;

typedef struct {
    bool    active, started;
    JobKind kind;
    int     id, slot;
    Asset   snap;                        /* copy of a deleted asset, so it can still be drawn */
    float   t;                           /* seconds since the job started */
    Vector3 pts[4];                      /* worker route: outside -> door -> aisle -> shelf */
    float   seg[3], total;
    float   placeEnd;                    /* deliveries: time the box lands on the shelf */
} Job;

#define MAX_JOBS 16
static Job jobs[MAX_JOBS];

static void job_build_path(Job *j, SlotInfo si)
{
    float startZ = (j->kind == JOB_DELIVER) ? WH_D + 0.65f : WH_D + 5.0f;
    j->pts[0] = (Vector3){ DOOR_X, 0.0f, startZ };
    j->pts[1] = (Vector3){ DOOR_X, 0.0f, WH_D };
    j->pts[2] = (Vector3){ DOOR_X, 0.0f, si.pickZ };
    j->pts[3] = (Vector3){ si.pickX, 0.0f, si.pickZ };
    j->total = 0.0f;
    for (int k = 0; k < 3; k++) {        /* every leg is axis-aligned, so length = |dx| + |dz| */
        j->seg[k] = fabsf(j->pts[k + 1].x - j->pts[k].x) + fabsf(j->pts[k + 1].z - j->pts[k].z);
        j->total += j->seg[k];
    }
}

/* Position (and heading in degrees) after walking distance d along the job's route. */
static Vector3 path_at(const Job *j, float d, float *heading)
{
    if (d < 0.0f) d = 0.0f;
    if (d > j->total) d = j->total;
    for (int k = 0; k < 3; k++) {
        if (d <= j->seg[k] || k == 2) {
            Vector3 a = j->pts[k], b = j->pts[k + 1];
            float f = (j->seg[k] > 0.0f) ? d / j->seg[k] : 1.0f;
            if (f > 1.0f) f = 1.0f;
            *heading = atan2f(b.x - a.x, b.z - a.z) * RAD2DEG;
            return (Vector3){ lerpf(a.x, b.x, f), 0.0f, lerpf(a.z, b.z, f) };
        }
        d -= j->seg[k];
    }
    *heading = 0.0f;
    return j->pts[3];
}

static void anim_clear(void) { memset(jobs, 0, sizeof(jobs)); }

static Job *job_alloc(void)
{
    for (int k = 0; k < MAX_JOBS; k++) if (!jobs[k].active) { memset(&jobs[k], 0, sizeof(Job)); return &jobs[k]; }
    return NULL;                         /* queue full: the box just appears / disappears */
}

/* Call right after a new asset has been added. */
static void anim_queue_delivery(int id)
{
    Job *j = job_alloc();
    if (!j) return;
    j->active = true;
    j->kind = JOB_DELIVER;
    j->id = id;
}

/* Call right before an asset is deleted. */
static void anim_queue_removal(const Asset *a)
{
    for (int k = 0; k < MAX_JOBS; k++) {         /* deleted before its box even arrived: cancel the delivery */
        Job *d = &jobs[k];
        if (d->active && d->kind == JOB_DELIVER && d->id == a->id && (!d->started || d->t < d->placeEnd)) {
            d->active = false;
            return;
        }
    }
    int slot = slot_of(a->id);
    if (slot < 0) return;
    Job *j = job_alloc();
    if (!j) return;
    j->active = j->started = true;
    j->kind = JOB_REMOVE;
    j->id = a->id;
    j->slot = slot;
    j->snap = *a;
    job_build_path(j, slot_info(slot));
}

/* True while an asset exists in the store but its box hasn't been put on the shelf yet. */
static bool box_hidden(int id)
{
    for (int k = 0; k < MAX_JOBS; k++) {
        const Job *j = &jobs[k];
        if (j->active && j->kind == JOB_DELIVER && j->id == id && (!j->started || j->t < j->placeEnd))
            return true;
    }
    return false;
}

static bool slot_held(int slot)
{
    for (int k = 0; k < MAX_JOBS; k++)
        if (jobs[k].active && jobs[k].kind == JOB_REMOVE && jobs[k].slot == slot) return true;
    return false;
}

/* Frees slots of deleted assets and gives new assets a slot. */
static void sync_slots(void)
{
    for (int s = 0; s < SLOT_COUNT; s++)
        if (slotId[s] != 0 && !store_find_by_id(slotId[s]) && !slot_held(s)) slotId[s] = 0;

    for (int i = 0; i < store_count(); i++) {
        int id = store_get(i)->id;
        if (slot_of(id) >= 0) continue;
        for (int s = 0; s < SLOT_COUNT; s++) if (slotId[s] == 0) { slotId[s] = id; break; }
    }
}

static void anim_update(float dt)
{
    if (dt > 0.05f) dt = 0.05f;

    bool laneBusy = false;               /* only one truck at the dock at a time */
    for (int k = 0; k < MAX_JOBS; k++)
        if (jobs[k].active && jobs[k].started && jobs[k].kind == JOB_DELIVER && jobs[k].t < TRUCK_CLEAR) laneBusy = true;

    for (int k = 0; k < MAX_JOBS; k++) {
        Job *j = &jobs[k];
        if (!j->active) continue;

        if (!j->started) {
            if (laneBusy) continue;
            j->slot = slot_of(j->id);
            if (j->slot < 0) { j->active = false; continue; }
            job_build_path(j, slot_info(j->slot));
            j->placeEnd = T_ARRIVE + T_UNLOAD + j->total / WALK_SPEED + T_PLACE;
            j->started = true;
            laneBusy = true;
        }

        j->t += dt;
        float endT;
        if (j->kind == JOB_DELIVER)
            endT = j->placeEnd + (j->total - j->seg[0]) / WALK_SPEED;
        else
            endT = 2.0f * j->total / WALK_SPEED + T_PICK;
        if (j->t >= endT) j->active = false;
    }
}

/* ---- drawing ---- */
static void draw_truck(float cx, float rz, Color body, Color cab)   /* rz = z of the rear bumper; cab at +z */
{
    Color dark = { 30, 41, 59, 255 }, glass = { 125, 180, 230, 255 };

    draw_box((Vector3){ cx, 0.40f, rz + 2.6f }, 2.0f, 0.2f, 5.2f, dark);               /* chassis */
    draw_box((Vector3){ cx, 1.60f, rz + 2.0f }, 2.3f, 2.2f, 3.8f, body);               /* cargo box */
    draw_box((Vector3){ cx, 1.60f, rz + 0.08f }, 2.0f, 2.0f, 0.04f, shade(body, 0.7f)); /* rear doors */
    draw_box((Vector3){ cx - 1.16f, 1.45f, rz + 2.0f }, 0.03f, 0.4f, 3.8f, cab);       /* side stripes */
    draw_box((Vector3){ cx + 1.16f, 1.45f, rz + 2.0f }, 0.03f, 0.4f, 3.8f, cab);
    draw_box((Vector3){ cx, 1.15f, rz + 4.55f }, 2.1f, 1.4f, 1.3f, cab);               /* cab */
    draw_box((Vector3){ cx, 1.45f, rz + 5.22f }, 1.8f, 0.6f, 0.05f, glass);            /* windscreen */

    float wz[2] = { rz + 0.9f, rz + 4.4f };
    for (int i = 0; i < 2; i++) {
        draw_box((Vector3){ cx - 1.05f, 0.35f, wz[i] }, 0.25f, 0.7f, 0.7f, dark);
        draw_box((Vector3){ cx + 1.05f, 0.35f, wz[i] }, 0.25f, 0.7f, 0.7f, dark);
    }
}

static float truck_rear(float t)
{
    if (t < T_ARRIVE) {                                      /* reversing in, slowing down */
        float s = t / T_ARRIVE, e = 1.0f - (1.0f - s) * (1.0f - s) * (1.0f - s);
        return TRUCK_FAR + (TRUCK_STOP - TRUCK_FAR) * e;
    }
    if (t < T_ARRIVE + T_UNLOAD) return TRUCK_STOP;
    float u = (t - T_ARRIVE - T_UNLOAD) / T_DEPART;          /* driving off, speeding up */
    if (u > 1.0f) u = 1.0f;
    return TRUCK_STOP + (TRUCK_FAR - TRUCK_STOP) * u * u;
}

static Vector3 hand_pos(Vector3 p, float headingDeg)
{
    float r = headingDeg * DEG2RAD;
    return (Vector3){ p.x + sinf(r) * 0.52f, 0.95f, p.z + cosf(r) * 0.52f };
}

static void draw_person(Vector3 p, float heading, float phase, float alpha, bool carry, Color boxCol, Color vest)
{
    unsigned char a = (unsigned char)(255.0f * (alpha < 0 ? 0 : (alpha > 1 ? 1 : alpha)));
    Color pants = { 51, 65, 85, a }, skin = { 253, 224, 185, a }, hat = { 250, 204, 21, a };
    vest.a = a;
    float swing = sinf(phase) * 0.12f;

    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(heading, 0.0f, 1.0f, 0.0f);
    draw_box((Vector3){ -0.10f, 0.25f,  swing }, 0.16f, 0.50f, 0.16f, pants);   /* legs */
    draw_box((Vector3){  0.10f, 0.25f, -swing }, 0.16f, 0.50f, 0.16f, pants);
    draw_box((Vector3){  0.00f, 0.78f, 0.00f }, 0.42f, 0.56f, 0.26f, vest);      /* torso */
    draw_box((Vector3){  0.00f, 1.20f, 0.00f }, 0.24f, 0.24f, 0.24f, skin);      /* head */
    draw_box((Vector3){  0.00f, 1.36f, 0.00f }, 0.28f, 0.10f, 0.28f, hat);       /* hard hat */
    if (carry) {
        boxCol.a = a;
        draw_box((Vector3){ 0.0f, 0.95f, 0.52f }, BOX_SIZE, BOX_SIZE, BOX_SIZE, boxCol);
    }
    rlPopMatrix();
}

/* ---- people wandering around the warehouse ----
 * They stay on the aisles: along a row aisle, onto the central aisle, and out along another
 * row aisle, then stand still for a moment before picking somewhere new to go. */
#define WALKER_COUNT 6
#define WALKER_SPEED 1.3f

typedef struct {
    Vector3 pos;
    Vector3 route[3];
    int     n, idx, row;
    float   heading, phase, wait;
    Color   vest;
} Walker;

static Walker walkers[WALKER_COUNT];
static bool   walkersReady = false;

static float aisle_z(int row) { return ROW0_Z + row * (RACK_D + ROW_GAP) + RACK_D + 0.9f; }
static float frand(float lo, float hi) { return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX); }

static void walkers_update(float dt)
{
    static const Color vests[WALKER_COUNT] = {
        { 34, 197, 94, 255 }, { 250, 204, 21, 255 }, { 56, 189, 248, 255 }, { 244, 114, 182, 255 },
        { 168, 85, 247, 255 }, { 248, 113, 113, 255 }
    };
    if (dt > 0.05f) dt = 0.05f;

    if (!walkersReady) {
        for (int i = 0; i < WALKER_COUNT; i++) {
            Walker *w = &walkers[i];
            memset(w, 0, sizeof(*w));
            w->row  = rand() % RACK_ROWS;
            w->pos  = (Vector3){ frand(MARGIN + 0.3f, WH_W - MARGIN - 0.3f), 0.0f, aisle_z(w->row) };
            w->wait = frand(0.0f, 2.0f);
            w->vest = vests[i];
        }
        walkersReady = true;
    }

    for (int i = 0; i < WALKER_COUNT; i++) {
        Walker *w = &walkers[i];

        if (w->wait > 0.0f) { w->wait -= dt; continue; }

        if (w->idx >= w->n) {                       /* pick a new destination */
            int   row = rand() % RACK_ROWS;
            float tx  = frand(MARGIN + 0.3f, WH_W - MARGIN - 0.3f);
            w->n = 0; w->idx = 0;
            if (row != w->row) {
                w->route[w->n++] = (Vector3){ DOOR_X, 0.0f, aisle_z(w->row) };
                w->route[w->n++] = (Vector3){ DOOR_X, 0.0f, aisle_z(row) };
            }
            w->route[w->n++] = (Vector3){ tx, 0.0f, aisle_z(row) };
            w->row = row;
        }

        Vector3 goal = w->route[w->idx];
        float dx = goal.x - w->pos.x, dz = goal.z - w->pos.z;
        float dist = fabsf(dx) + fabsf(dz);          /* legs are axis-aligned */
        float step = WALKER_SPEED * dt;

        if (dist <= step) {
            w->pos = goal;
            w->idx++;
            if (w->idx >= w->n) w->wait = frand(0.6f, 2.5f);
        } else {
            if (dist > 0.0001f) w->heading = atan2f(dx, dz) * RAD2DEG;
            w->pos.x += (dx > 0 ? 1.0f : -1.0f) * fminf(fabsf(dx), step);
            if (fabsf(dx) < step) w->pos.z += (dz > 0 ? 1.0f : -1.0f) * fminf(fabsf(dz), step - fabsf(dx));
            w->phase += dt * 7.0f;
        }
    }
}

static void draw_walkers(void)
{
    for (int i = 0; i < WALKER_COUNT; i++) {
        const Walker *w = &walkers[i];
        draw_person(w->pos, w->heading, w->phase, 1.0f, false, WHITE, w->vest);
    }
}

/* ---- scenery around the warehouse ---- */
static float hash01(int n)               /* stable pseudo-random 0..1 (same every frame) */
{
    unsigned x = (unsigned)n * 2654435761u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
    return (float)(x & 0xFFFFu) / 65535.0f;
}

/* A truck at (cx, cz) seen from above, rotated by headingDeg (0 = cab pointing +z). */
static void draw_truck_at(float cx, float cz, float headingDeg, Color body, Color cab)
{
    rlPushMatrix();
    rlTranslatef(cx, 0.0f, cz);
    rlRotatef(headingDeg, 0.0f, 1.0f, 0.0f);
    draw_truck(0.0f, -2.6f, body, cab);
    rlPopMatrix();
}

static void draw_car(Vector3 p, float headingDeg, Color body)
{
    Color dark = { 30, 41, 59, 255 }, glass = { 125, 180, 230, 255 };
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(headingDeg, 0.0f, 1.0f, 0.0f);
    draw_box((Vector3){ 0.0f, 0.45f,  0.0f }, 1.0f, 0.5f, 2.2f, body);
    draw_box((Vector3){ 0.0f, 0.90f, -0.1f }, 0.9f, 0.4f, 1.1f, glass);
    draw_box((Vector3){ 0.0f, 1.13f, -0.1f }, 0.92f, 0.06f, 1.12f, body);
    for (int i = 0; i < 4; i++)
        draw_box((Vector3){ (i % 2 ? 0.5f : -0.5f), 0.2f, (i / 2 ? 0.7f : -0.7f) }, 0.14f, 0.4f, 0.4f, dark);
    rlPopMatrix();
}

static void draw_container(float x, float z, Color col, bool stacked)
{
    for (int level = 0; level < (stacked ? 2 : 1); level++) {
        Color c = level ? shade(col, 0.85f) : col;
        float y = 1.3f + level * 2.6f;
        draw_box((Vector3){ x, y, z }, 2.4f, 2.6f, 6.0f, c);
        draw_box((Vector3){ x, y, z + 3.01f }, 2.2f, 2.4f, 0.04f, shade(c, 0.6f));   /* doors */
        draw_box((Vector3){ x, y + 1.31f, z }, 2.0f, 0.04f, 5.6f, shade(c, 1.15f));  /* roof rib */
    }
}

static void draw_tree(float x, float z, float scale, Color leaf)
{
    Color trunk = { 120, 85, 55, 255 };
    draw_box((Vector3){ x, 0.55f * scale, z }, 0.3f * scale, 1.1f * scale, 0.3f * scale, trunk);
    draw_box((Vector3){ x, 1.6f * scale, z }, 1.8f * scale, 1.3f * scale, 1.8f * scale, leaf);
    draw_box((Vector3){ x, 2.5f * scale, z }, 1.2f * scale, 1.0f * scale, 1.2f * scale, shade(leaf, 1.12f));
}

static void draw_pole(float x, float z)
{
    draw_box((Vector3){ x, 2.6f, z }, 0.14f, 5.2f, 0.14f, (Color){ 90, 100, 115, 255 });
    draw_box((Vector3){ x, 5.3f, z }, 0.7f, 0.15f, 0.35f, (Color){ 250, 240, 190, 255 });
}

static void draw_office(float x, float z)
{
    Color wall = darkMode ? (Color){ 100, 116, 139, 255 } : (Color){ 226, 232, 240, 255 };
    Color dark = { 30, 41, 59, 255 }, glass = { 125, 180, 230, 255 };
    draw_box((Vector3){ x, 1.6f, z }, 5.0f, 3.2f, 4.0f, wall);
    draw_box((Vector3){ x, 3.33f, z }, 5.4f, 0.25f, 4.4f, (Color){ 71, 85, 105, 255 });
    draw_box((Vector3){ x - 1.6f, 1.0f, z + 2.01f }, 0.9f, 2.0f, 0.04f, dark);
    draw_box((Vector3){ x + 0.2f, 1.8f, z + 2.01f }, 1.0f, 0.9f, 0.04f, glass);
    draw_box((Vector3){ x + 1.6f, 1.8f, z + 2.01f }, 1.0f, 0.9f, 0.04f, glass);
}

static void fence_run(float x0, float z0, float x1, float z1)
{
    Color post = { 120, 113, 108, 255 }, rail = { 160, 160, 170, 255 };
    float len = fabsf(x1 - x0) + fabsf(z1 - z0);
    float dx = (x1 - x0) / len, dz = (z1 - z0) / len;
    int n = (int)(len / 3.5f);
    for (int i = 0; i <= n; i++)
        draw_box((Vector3){ x0 + dx * i * 3.5f, 0.6f, z0 + dz * i * 3.5f }, 0.12f, 1.2f, 0.12f, post);
    float sx = fabsf(x1 - x0) > 0 ? fabsf(x1 - x0) : 0.05f, sz = fabsf(z1 - z0) > 0 ? fabsf(z1 - z0) : 0.05f;
    draw_box((Vector3){ (x0 + x1) / 2, 0.45f, (z0 + z1) / 2 }, sx, 0.05f, sz, rail);
    draw_box((Vector3){ (x0 + x1) / 2, 0.95f, (z0 + z1) / 2 }, sx, 0.05f, sz, rail);
}

static void draw_scenery(void)
{
    Color grass   = darkMode ? (Color){ 24, 54, 40, 255 }   : (Color){ 130, 185, 115, 255 };
    Color asphalt = darkMode ? (Color){ 30, 41, 59, 255 }   : (Color){ 120, 130, 145, 255 };
    Color leaf    = darkMode ? (Color){ 22, 101, 52, 255 }  : (Color){ 46, 139, 87, 255 };
    Color white = { 245, 245, 245, 255 }, yellow = { 250, 204, 21, 255 };

    const float gx0 = -50.0f, gx1 = WH_W + 55.0f, gz0 = -40.0f, gz1 = WH_D + 60.0f;
    const float roadZ = WH_D + 26.0f;
    const float yardX = WH_W + 11.0f, yardZ = WH_D - 1.0f;       /* truck yard centre (east) */
    const float lotX = -12.0f, lotZ = WH_D;                       /* staff car park centre (west) */

    /* ground, road, yard and car park */
    draw_box((Vector3){ (gx0 + gx1) / 2, -0.16f, (gz0 + gz1) / 2 }, gx1 - gx0, 0.2f, gz1 - gz0, grass);
    draw_box((Vector3){ (gx0 + gx1) / 2, -0.1f, roadZ }, gx1 - gx0, 0.2f, 8.0f, asphalt);
    for (float x = gx0 + 2; x < gx1; x += 5.0f)
        draw_box((Vector3){ x, 0.01f, roadZ }, 2.4f, 0.02f, 0.15f, yellow);
    draw_box((Vector3){ yardX, -0.1f, yardZ }, 16.0f, 0.2f, 18.0f, asphalt);
    draw_box((Vector3){ yardX, -0.1f, WH_D + 15.0f }, 4.0f, 0.2f, 14.0f, asphalt);
    draw_box((Vector3){ lotX, -0.1f, lotZ }, 16.0f, 0.2f, 20.0f, asphalt);
    draw_box((Vector3){ lotX, -0.1f, WH_D + 16.0f }, 4.0f, 0.2f, 12.0f, asphalt);

    /* idle trucks parked in the yard */
    static const Color bodies[4] = { { 236, 240, 245, 255 }, { 250, 204, 21, 255 }, { 203, 213, 225, 255 }, { 59, 130, 246, 255 } };
    static const Color cabs[4]   = { { 220, 38, 38, 255 }, { 22, 163, 74, 255 }, { 234, 88, 12, 255 }, { 30, 64, 175, 255 } };
    for (int i = 0; i < 4; i++)
        draw_truck_at(WH_W + 5.2f + i * 3.6f, WH_D - 6.0f, (i % 2) ? 180.0f : 0.0f, bodies[i], cabs[(i + 1) % 4]);
    for (int i = 0; i < 3; i++)
        draw_truck_at(WH_W + 7.0f + i * 3.6f, WH_D + 1.5f, (i % 2) ? 0.0f : 180.0f, bodies[(i + 2) % 4], cabs[i]);

    /* staff car park */
    static const Color carCols[6] = { { 220, 38, 38, 255 }, { 37, 99, 235, 255 }, { 226, 232, 240, 255 },
                                      { 51, 65, 85, 255 }, { 245, 158, 11, 255 }, { 22, 163, 74, 255 } };
    for (int row = 0; row < 3; row++) {
        float cz = lotZ - 6.5f + row * 6.0f;
        for (int k = 0; k < 6; k++)
            draw_box((Vector3){ -19.9f + k * 3.0f, 0.01f, cz }, 0.08f, 0.02f, 2.8f, white);
        for (int k = 0; k < 5; k++) {
            int h = row * 10 + k;
            if (hash01(h + 7) < 0.3f) continue;                  /* empty space */
            draw_car((Vector3){ -18.4f + k * 3.0f, 0.0f, cz }, hash01(h + 31) < 0.5f ? 0.0f : 180.0f,
                     carCols[(int)(hash01(h + 3) * 5.99f)]);
        }
    }

    /* shipping containers behind the yard */
    static const Color boxCols[5] = { { 200, 60, 50, 255 }, { 40, 100, 170, 255 }, { 60, 140, 80, 255 },
                                      { 220, 160, 40, 255 }, { 120, 120, 130, 255 } };
    for (int c = 0; c < 4; c++)
        draw_container(WH_W + 5.0f + (c % 2) * 3.5f, 4.2f + (c / 2) * 6.2f,
                       boxCols[(int)(hash01(c + 50) * 4.99f)], hash01(c + 90) < 0.5f);

    draw_office(-12.0f, WH_D + 15.0f);

    /* bushes along the front of the building */
    for (float x = 1.2f; x < WH_W - 1.0f; x += 1.7f) {
        if (fabsf(x - DOOR_X) < DOOR_W / 2 + 1.2f) continue;
        draw_box((Vector3){ x, 0.35f, WH_D + 1.1f }, 1.1f, 0.7f, 0.8f, shade(leaf, 0.9f));
    }

    /* street lights */
    static const float poles[][2] = {
        { DOOR_X - 4.5f, WH_D + 5.0f }, { DOOR_X + 4.5f, WH_D + 5.0f },
        { DOOR_X - 4.5f, WH_D + 15.0f }, { DOOR_X + 4.5f, WH_D + 15.0f },
        { WH_W + 3.2f, WH_D - 9.0f }, { WH_W + 18.8f, WH_D - 9.0f },
        { WH_W + 3.2f, WH_D + 7.0f }, { WH_W + 18.8f, WH_D + 7.0f },
        { -19.5f, WH_D - 9.0f }, { -4.5f, WH_D - 9.0f }, { -19.5f, WH_D + 9.0f }, { -4.5f, WH_D + 9.0f }
    };
    for (int i = 0; i < (int)(sizeof(poles) / sizeof(poles[0])); i++) draw_pole(poles[i][0], poles[i][1]);

    /* perimeter fence (open at the road) */
    fence_run(-26.0f, -12.0f, WH_W + 26.0f, -12.0f);
    fence_run(-26.0f, -12.0f, -26.0f, WH_D + 21.0f);
    fence_run(WH_W + 26.0f, -12.0f, WH_W + 26.0f, WH_D + 21.0f);

    /* trees, scattered on a jittered grid but kept off the buildings, roads and car parks */
    const float keep[][4] = {                                    /* x0, z0, x1, z1 */
        { -4.0f, -4.0f, WH_W + 4.0f, WH_D + 4.0f },                          /* warehouse */
        { DOOR_X - 5.0f, WH_D, DOOR_X + 5.0f, WH_D + 23.0f },                /* loading apron */
        { gx0, WH_D + 21.0f, gx1, WH_D + 31.0f },                            /* road */
        { WH_W + 2.0f, WH_D - 11.0f, WH_W + 20.0f, WH_D + 23.0f },           /* yard + driveway */
        { -21.0f, WH_D - 11.0f, -3.0f, WH_D + 23.0f },                       /* car park + driveway */
        { -16.0f, WH_D + 11.0f, -8.0f, WH_D + 19.0f },                       /* office */
        { WH_W + 2.0f, 0.0f, WH_W + 12.0f, 15.0f }                           /* containers */
    };
    for (float gx = gx0 + 2; gx < gx1 - 2; gx += 5.5f) {
        for (float gz = gz0 + 2; gz < gz1 - 2; gz += 5.5f) {
            int cell = (int)((gx - gx0) / 5.5f) * 97 + (int)((gz - gz0) / 5.5f);
            if (hash01(cell) > 0.4f) continue;
            float x = gx + (hash01(cell + 1000) - 0.5f) * 3.5f;
            float z = gz + (hash01(cell + 2000) - 0.5f) * 3.5f;

            bool blocked = false;
            for (int r = 0; r < (int)(sizeof(keep) / sizeof(keep[0])); r++)
                if (x > keep[r][0] && x < keep[r][2] && z > keep[r][1] && z < keep[r][3]) { blocked = true; break; }
            if (blocked) continue;
            if (z > -14.5f && z < WH_D + 22.0f &&                  /* keep clear of the fence line */
                (fabsf(x + 26.0f) < 2.0f || fabsf(x - (WH_W + 26.0f)) < 2.0f)) continue;
            if (x > -28.0f && x < WH_W + 28.0f && fabsf(z + 12.0f) < 2.0f) continue;

            draw_tree(x, z, 0.8f + hash01(cell + 3000) * 0.7f, shade(leaf, 0.85f + hash01(cell + 4000) * 0.3f));
        }
    }

    /* traffic on the road: two cars heading east, one truck heading west */
    float tt = (float)GetTime(), span = gx1 - gx0;
    draw_car((Vector3){ gx0 + fmodf(tt * 6.0f + 10.0f, span), 0.0f, roadZ - 2.0f }, 90.0f, carCols[0]);
    draw_car((Vector3){ gx0 + fmodf(tt * 6.0f + 70.0f, span), 0.0f, roadZ - 2.0f }, 90.0f, carCols[1]);
    draw_truck_at(gx1 - fmodf(tt * 5.0f + 30.0f, span), roadZ + 2.0f, -90.0f, bodies[2], cabs[1]);
}

static void draw_warehouse(Vector3 cam)
{
    Color floorC = darkMode ? (Color){ 45, 56, 75, 255 }   : (Color){ 214, 222, 232, 255 };
    Color wallC  = darkMode ? (Color){ 71, 85, 105, 255 }  : (Color){ 148, 163, 184, 255 };
    Color roadC  = darkMode ? (Color){ 30, 41, 59, 255 }   : (Color){ 120, 130, 145, 255 };
    Color upright = { 37, 99, 235, 255 }, beam = { 234, 120, 30, 255 }, plank = { 196, 160, 110, 255 };
    Color yellow = { 250, 204, 21, 255 }, white = { 245, 245, 245, 255 };

    /* floor, loading apron outside the door, and floor markings */
    draw_box((Vector3){ WH_W / 2, -0.1f, WH_D / 2 }, WH_W, 0.2f, WH_D, floorC);
    draw_box((Vector3){ DOOR_X, -0.1f, WH_D + 11.0f }, 6.4f, 0.2f, 22.0f, roadC);
    for (int k = 0; k < 9; k++)
        draw_box((Vector3){ DOOR_X, 0.01f, WH_D + 2.0f + k * 2.0f }, 0.12f, 0.02f, 1.0f, white);
    draw_box((Vector3){ DOOR_X - AISLE_W / 2, 0.01f, WH_D / 2 }, 0.08f, 0.02f, WH_D - 0.4f, yellow);
    draw_box((Vector3){ DOOR_X + AISLE_W / 2, 0.01f, WH_D / 2 }, 0.08f, 0.02f, WH_D - 0.4f, yellow);

    /* shelving racks: blue uprights, orange beams, wooden shelf boards */
    for (int rack = 0; rack < RACK_ROWS * 2; rack++) {
        int row = rack / 2, side = rack % 2;
        float x0 = MARGIN + side * (RACK_LEN + AISLE_W);
        float z0 = ROW0_Z + row * (RACK_D + ROW_GAP);

        for (int k = 0; k <= BAYS_PER_RACK; k++) {
            draw_box((Vector3){ x0 + k * BAY_W, RACK_H / 2, z0 + 0.06f }, 0.1f, RACK_H, 0.1f, upright);
            draw_box((Vector3){ x0 + k * BAY_W, RACK_H / 2, z0 + RACK_D - 0.06f }, 0.1f, RACK_H, 0.1f, upright);
        }
        for (int level = 0; level < 3; level++) {
            float y = level * LEVEL_H;
            draw_box((Vector3){ x0 + RACK_LEN / 2, y + PLANK_T / 2, z0 + RACK_D / 2 }, RACK_LEN, PLANK_T, RACK_D, plank);
            draw_box((Vector3){ x0 + RACK_LEN / 2, y + 0.1f, z0 + 0.04f }, RACK_LEN, 0.12f, 0.05f, beam);
            draw_box((Vector3){ x0 + RACK_LEN / 2, y + 0.1f, z0 + RACK_D - 0.04f }, RACK_LEN, 0.12f, 0.05f, beam);
        }
    }

    /* walls: a wall is only drawn when the camera is on the inside of it, so it never blocks the view */
    if (cam.z > 0.0f)
        draw_box((Vector3){ WH_W / 2, WALL_H / 2, -WALL_T / 2 }, WH_W + 2 * WALL_T, WALL_H, WALL_T, wallC);
    if (cam.x > 0.0f)
        draw_box((Vector3){ -WALL_T / 2, WALL_H / 2, WH_D / 2 }, WALL_T, WALL_H, WH_D + 2 * WALL_T, wallC);
    if (cam.x < WH_W)
        draw_box((Vector3){ WH_W + WALL_T / 2, WALL_H / 2, WH_D / 2 }, WALL_T, WALL_H, WH_D + 2 * WALL_T, wallC);
    if (cam.z < WH_D) {                                      /* front wall, with the loading door cut out */
        float lw = DOOR_X - DOOR_W / 2, rw = WH_W - (DOOR_X + DOOR_W / 2);
        draw_box((Vector3){ lw / 2, WALL_H / 2, WH_D + WALL_T / 2 }, lw, WALL_H, WALL_T, wallC);
        draw_box((Vector3){ WH_W - rw / 2, WALL_H / 2, WH_D + WALL_T / 2 }, rw, WALL_H, WALL_T, wallC);
        draw_box((Vector3){ DOOR_X, (DOOR_H + WALL_H) / 2, WH_D + WALL_T / 2 }, DOOR_W, WALL_H - DOOR_H, WALL_T, wallC);
        draw_box((Vector3){ DOOR_X - DOOR_W / 2, DOOR_H / 2, WH_D + WALL_T / 2 }, 0.14f, DOOR_H, WALL_T + 0.1f, yellow);
        draw_box((Vector3){ DOOR_X + DOOR_W / 2, DOOR_H / 2, WH_D + WALL_T / 2 }, 0.14f, DOOR_H, WALL_T + 0.1f, yellow);
    }
}

/* Trucks, workers, and the boxes they are carrying. */
static void draw_jobs(void)
{
    for (int k = 0; k < MAX_JOBS; k++) {
        const Job *j = &jobs[k];
        if (!j->active || !j->started) continue;

        float t = j->t, walkT = j->total / WALK_SPEED;
        SlotInfo si = slot_info(j->slot);
        Vector3 slotC = { si.pos.x, si.pos.y + BOX_SIZE / 2, si.pos.z };
        Vector3 pos = j->pts[0], boxPos = slotC;
        float heading = 180.0f, alpha = 1.0f;
        bool show = true, carry = false, looseBox = false;
        Color bc;

        if (j->kind == JOB_DELIVER) {
            const Asset *a = store_find_by_id(j->id);
            bc = STATUS_COLORS[a ? a->status : STATUS_IN_STORAGE];

            float walkStart = T_ARRIVE + T_UNLOAD, placeStart = walkStart + walkT;
            float endT = j->placeEnd + (j->total - j->seg[0]) / WALK_SPEED;

            if (t < TRUCK_CLEAR) draw_truck(DOOR_X, truck_rear(t), (Color){ 236, 240, 245, 255 }, T.accent);

            if (t < T_ARRIVE) {
                show = false;                                         /* still inside the truck */
            } else if (t < walkStart) {
                pos = j->pts[0]; heading = 180.0f; carry = true;      /* waiting at the truck with the box */
            } else if (t < placeStart) {
                pos = path_at(j, (t - walkStart) * WALK_SPEED, &heading); carry = true;
            } else if (t < j->placeEnd) {                             /* reach out and set the box on the shelf */
                pos = j->pts[3]; heading = 180.0f;
                Vector3 hand = hand_pos(pos, heading);
                float s = smooth((t - placeStart) / T_PLACE);
                boxPos = (Vector3){ lerpf(hand.x, slotC.x, s), lerpf(hand.y, slotC.y, s), lerpf(hand.z, slotC.z, s) };
                looseBox = true;
            } else {
                float d = j->total - (t - j->placeEnd) * WALK_SPEED;  /* heading back out empty-handed */
                if (d < j->seg[0]) d = j->seg[0];
                pos = path_at(j, d, &heading);
                heading += 180.0f;
                alpha = fminf(1.0f, (endT - t) / T_FADE);
            }
        } else {
            bc = STATUS_COLORS[j->snap.status];
            float pickEnd = walkT + T_PICK, endT = pickEnd + walkT;
            alpha = fminf(fminf(1.0f, t / T_FADE), fminf(1.0f, (endT - t) / T_FADE));

            if (t < walkT) {                                          /* walking in; box still on its shelf */
                pos = path_at(j, t * WALK_SPEED, &heading);
                boxPos = slotC; looseBox = true;
            } else if (t < pickEnd) {                                 /* lift the box off the shelf */
                pos = j->pts[3]; heading = 180.0f;
                Vector3 hand = hand_pos(pos, heading);
                float s = smooth((t - walkT) / T_PICK);
                boxPos = (Vector3){ lerpf(slotC.x, hand.x, s), lerpf(slotC.y, hand.y, s), lerpf(slotC.z, hand.z, s) };
                looseBox = true;
            } else {                                                  /* carrying it out */
                float d = j->total - (t - pickEnd) * WALK_SPEED;
                pos = path_at(j, d, &heading);
                heading += 180.0f;
                carry = true;
            }
        }

        if (looseBox) draw_box(boxPos, BOX_SIZE, BOX_SIZE, BOX_SIZE, bc);
        if (show) draw_person(pos, heading, t * 9.0f, alpha, carry, bc, (Color){ 250, 160, 20, 255 });
    }
}

static void draw_assets_3d(Rectangle view)
{
    int n = store_count();
    int w = (int)view.width  < 1 ? 1 : (int)view.width;
    int h = (int)view.height < 1 ? 1 : (int)view.height;

    /* (re)create the render target whenever the panel size changes */
    if (sceneRT.id == 0 || w != sceneW || h != sceneH) {
        if (sceneRT.id != 0) UnloadRenderTexture(sceneRT);
        sceneRT = LoadRenderTexture(w, h);
        sceneW = w; sceneH = h;
    }

    sync_slots();
    anim_update(GetFrameTime());
    walkers_update(GetFrameTime());

    Vector3 center = { DOOR_X, 1.2f, WH_D / 2.0f - 1.0f };
    if (camDist <= 0.0f) camDist = 42.0f;

    /* which box lives in which slot, and is it on its shelf yet? */
    int  idxSlot[MAX_ASSETS];
    bool shown[MAX_ASSETS];
    for (int i = 0; i < n; i++) {
        int id = store_get(i)->id;
        idxSlot[i] = slot_of(id);
        shown[i]   = idxSlot[i] >= 0 && !box_hidden(id);
    }

    Rectangle resetBtn = { view.x + 12,  view.y + 12, 110, 30 };
    Rectangle fsBtn    = { view.x + 130, view.y + 12, 150, 30 };

    /* ---- input: orbit + zoom ---- */
    Vector2 mouse  = GetMousePosition();
    bool    inside = !confirmDelete && CheckCollisionPointRec(mouse, view) &&
                     !CheckCollisionPointRec(mouse, resetBtn) && !CheckCollisionPointRec(mouse, fsBtn);

    if (inside) {
        camDist *= 1.0f - GetMouseWheelMove() * 0.08f;
        if (camDist < 5.0f)  camDist = 5.0f;
        if (camDist > 110.0f) camDist = 110.0f;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            orbiting = true;
            dragDist = 0.0f;
        }
    }
    if (orbiting && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT))) {
        Vector2 d = GetMouseDelta();
        dragDist += fabsf(d.x) + fabsf(d.y);
        if (dragDist > 4.0f) {                       /* small wobbles still count as a click */
            camYaw   -= d.x * 0.007f;
            camPitch += d.y * 0.007f;
            if (camPitch < 0.12f) camPitch = 0.12f;
            if (camPitch > 1.45f) camPitch = 1.45f;
        }
    }
    bool dragging = orbiting && dragDist > 4.0f;
    bool clicked  = orbiting && dragDist <= 4.0f && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) orbiting = false;

    if (!confirmDelete && !searchEditing) {          /* don't steal keys while typing in the search box */
        if (IsKeyPressed(KEY_R)) { camYaw = 0.6f; camPitch = 0.65f; camDist = 0.0f; }
        if (IsKeyPressed(KEY_F)) fullView3D = !fullView3D;
    }
    if (!confirmDelete && fullView3D && IsKeyPressed(KEY_ESCAPE)) fullView3D = false;

    /* ---- camera ---- */
    Camera3D cam = { 0 };
    cam.target     = center;
    cam.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy       = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    cam.position   = (Vector3){
        center.x + camDist * cosf(camPitch) * sinf(camYaw),
        center.y + camDist * sinf(camPitch),
        center.z + camDist * cosf(camPitch) * cosf(camYaw)
    };

    /* ---- picking: which box is under the mouse? ---- */
    int hoverIdx = -1;
    if (inside && !dragging && n > 0) {
        Ray ray = GetScreenToWorldRayEx((Vector2){ mouse.x - view.x, mouse.y - view.y }, cam, w, h);
        float best = 1e30f;
        for (int i = 0; i < n; i++) {
            if (!shown[i]) continue;
            SlotInfo si = slot_info(idxSlot[i]);
            BoundingBox bb = {
                { si.pos.x - BOX_SIZE / 2, si.pos.y,            si.pos.z - BOX_SIZE / 2 },
                { si.pos.x + BOX_SIZE / 2, si.pos.y + BOX_SIZE, si.pos.z + BOX_SIZE / 2 }
            };
            RayCollision rc = GetRayCollisionBox(ray, bb);
            if (rc.hit && rc.distance < best) { best = rc.distance; hoverIdx = i; }
        }
    }
    if (clicked && inside) selectedId = (hoverIdx >= 0) ? store_get(hoverIdx)->id : -1;

    /* ---- render the scene into the texture ---- */
    BeginTextureMode(sceneRT);
    ClearBackground(T.bg);
    BeginMode3D(cam);

    rlDisableBackfaceCulling();
    draw_scenery();
    draw_warehouse(cam.position);

    for (int i = 0; i < n; i++) {
        if (!shown[i]) continue;
        Asset   *a  = store_get(i);
        SlotInfo si = slot_info(idxSlot[i]);
        bool dimmed = (searchText[0] != '\0') && !matches_search(a);

        Color col = STATUS_COLORS[a->status];
        if (dimmed)             col = mix(col, T.card, 0.75f);
        else if (i == hoverIdx) col = shade(col, 1.2f);
        draw_box((Vector3){ si.pos.x, si.pos.y + BOX_SIZE / 2, si.pos.z }, BOX_SIZE, BOX_SIZE, BOX_SIZE, col);
    }

    draw_jobs();
    draw_walkers();
    rlEnableBackfaceCulling();

    float t = (float)GetTime();
    for (int i = 0; i < n; i++) {
        if (!shown[i]) continue;
        Asset   *a  = store_get(i);
        SlotInfo si = slot_info(idxSlot[i]);
        Urgency  u  = store_urgency(a, nowT);
        Vector3  c  = { si.pos.x, si.pos.y + BOX_SIZE / 2, si.pos.z };

        if (a->id == selectedId) DrawCubeWires(c, BOX_SIZE + 0.08f, BOX_SIZE + 0.08f, BOX_SIZE + 0.08f, T.text);
        else if (u == URGENCY_OVERDUE) DrawCubeWires(c, BOX_SIZE, BOX_SIZE, BOX_SIZE, T.error);

        if (u == URGENCY_OVERDUE || u == URGENCY_SOON) {          /* bobbing marker above the box */
            float bob = 0.04f * sinf(t * 3.0f + (float)i);
            DrawSphereEx((Vector3){ si.pos.x, si.pos.y + BOX_SIZE + 0.22f + bob, si.pos.z }, 0.1f, 10, 10, urgency_color(u));
        }
    }

    EndMode3D();
    EndTextureMode();

    /* ---- panel frame + scene ---- */
    DrawTextureRec(sceneRT.texture, (Rectangle){ 0, 0, (float)w, (float)-h }, (Vector2){ view.x, view.y }, WHITE);
    DrawRectangleLinesEx(view, 1, T.line);

    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);

    /* asset IDs floating above each box (hidden when zoomed far out, where they'd just be clutter) */
    if (camDist < 65.0f) {
        for (int i = 0; i < n; i++) {
            if (!shown[i]) continue;
            Asset   *a  = store_get(i);
            SlotInfo si = slot_info(idxSlot[i]);
            Urgency  u  = store_urgency(a, nowT);
            float labelY = si.pos.y + BOX_SIZE + ((u == URGENCY_OVERDUE || u == URGENCY_SOON) ? 0.55f : 0.3f);
            Vector2 sp = GetWorldToScreenEx((Vector3){ si.pos.x, labelY, si.pos.z }, cam, w, h);
            const char *id = TextFormat("%d", a->id);
            DrawText(id, (int)(view.x + sp.x) - MeasureText(id, 14) / 2, (int)(view.y + sp.y) - 7, 14, T.text);
        }
    }

    if (n == 0)
        DrawText("The warehouse is empty - add an asset and a truck will deliver it.",
                 (int)view.x + 20, (int)view.y + 60, 18, T.muted);

    /* details card for the hovered asset (or the selected one) */
    int focus = hoverIdx;
    if (focus < 0 && selectedId >= 0) {
        for (int i = 0; i < n; i++) if (store_get(i)->id == selectedId && shown[i]) { focus = i; break; }
    }
    if (focus >= 0) {
        Asset *a = store_get(focus);
        Rectangle card = { view.x + view.width - 292, view.y + 12, 280, 120 };
        DrawRectangleRounded(card, 0.08f, 8, T.card);
        DrawRectangleRoundedLines(card, 0.08f, 8, T.line);

        char due[16], cd[48], line[160];
        store_format_date(a->deadline, due, sizeof(due));
        store_countdown_text(a, nowT, cd, sizeof(cd));
        Urgency u = store_urgency(a, nowT);

        draw_text_fit(a->name, card.x + 14, card.y + 10, card.width - 28, 20, T.text);
        snprintf(line, sizeof(line), "%s  -  %s", a->category[0] ? a->category : "No category",
                 a->assignedTo[0] ? a->assignedTo : "Unassigned");
        draw_text_fit(line, card.x + 14, card.y + 38, card.width - 28, 16, T.muted);
        draw_text_fit(STATUS_NAME(a->status), card.x + 14, card.y + 62, card.width - 28, 16, STATUS_COLORS[a->status]);
        snprintf(line, sizeof(line), "%s  (%s)", due[0] ? due : "No deadline", cd);
        draw_text_fit(line, card.x + 14, card.y + 88, card.width - 28, 16, u == URGENCY_NONE ? T.muted : urgency_color(u));
    }

    /* legend + controls hint */
    DrawText("Drag: orbit   Scroll: zoom   Click: select   R: reset   F: fullscreen",
             (int)view.x + 14, (int)(view.y + view.height) - 52, 16, T.muted);
    float lx = view.x + 14, ly = view.y + view.height - 28;
    for (int s = 0; s < STATUS_COUNT; s++) {
        DrawRectangle((int)lx, (int)ly + 2, 14, 14, STATUS_COLORS[s]);
        DrawText(STATUS_NAME(s), (int)lx + 20, (int)ly, 16, T.text);
        lx += 20 + MeasureText(STATUS_NAME(s), 16) + 22;
    }
    DrawCircle((int)lx + 7, (int)ly + 9, 6, T.error);
    DrawText("Overdue", (int)lx + 20, (int)ly, 16, T.text);
    lx += 20 + MeasureText("Overdue", 16) + 22;
    DrawCircle((int)lx + 7, (int)ly + 9, 6, T.warn);
    DrawText("Due soon", (int)lx + 20, (int)ly, 16, T.text);

    EndScissorMode();

    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    if (GuiButton(resetBtn, "Reset view")) { camYaw = 0.6f; camPitch = 0.65f; camDist = 0.0f; }
    if (GuiButton(fsBtn, fullView3D ? "Exit fullscreen" : "Fullscreen")) fullView3D = !fullView3D;
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
}

/* ---------- asset list ---------- */
static void draw_assets(Rectangle area)
{
    if (view3D && fullView3D) {          /* fullscreen: the warehouse takes over the whole window */
        draw_assets_3d((Rectangle){ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() });
        return;
    }

    DrawText("Assets", (int)area.x, (int)area.y, 30, T.text);
    if (notice[0] != '\0')
        DrawText(notice, (int)area.x + 140, (int)area.y + 8, 18, T.error);

    /* toolbar */
    float toolbarY = area.y + 50;
    Rectangle searchBox = { area.x, toolbarY, 170, 36 };
    Rectangle viewBtn   = { area.x + 178, toolbarY, 96, 36 };
    Rectangle addBtn    = { area.x + area.width - 350, toolbarY, 140, 36 };
    Rectangle editBtn   = { area.x + area.width - 200, toolbarY,  90, 36 };
    Rectangle delBtn    = { area.x + area.width - 100, toolbarY, 100, 36 };

    if (GuiTextBox(searchBox, searchText, (int)sizeof(searchText), searchEditing))
        searchEditing = !searchEditing;
    if (searchText[0] == '\0' && !searchEditing)
        DrawText("Search...", (int)searchBox.x + 10, (int)searchBox.y + 8, 20, T.muted);

    if (GuiButton(viewBtn, view3D ? "2D List" : "3D View")) {
        view3D = !view3D;
        orbiting = false;
    }

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
            store_format_date(a->deadline, deadlineBuf, sizeof(deadlineBuf));
            editingId = a->id;
            currentScreen = SCREEN_EDIT_ASSET;
            return;
        }
    }
    if (delClicked) {                 /* don't delete yet - ask first */
        confirmDelete = true;
        confirmId = selectedId;
    }

    if (view3D) {
        Rectangle view = { area.x, toolbarY + 52, area.width, area.height - (toolbarY + 52 - area.y) };
        draw_assets_3d(view);
        return;
    }

    /* table geometry */
    Rectangle table = { area.x, toolbarY + 52, area.width, area.height - (toolbarY + 52 - area.y) };
    Rectangle body  = { table.x, table.y + ROW_H, table.width, table.height - ROW_H };

    enum { COLS = 7 };
    float colW[COLS];
    colW[0] = 50;
    colW[1] = table.width * 0.20f;
    colW[2] = table.width * 0.11f;
    colW[3] = table.width * 0.15f;
    colW[4] = table.width * 0.11f;
    colW[5] = table.width * 0.13f;
    colW[6] = table.width - colW[0] - colW[1] - colW[2] - colW[3] - colW[4] - colW[5];

    float colX[COLS];
    colX[0] = table.x;
    for (int i = 1; i < COLS; i++) colX[i] = colX[i - 1] + colW[i - 1];
    const char *headers[COLS] = { "ID", "Name", "Category", "Assigned to", "Status", "Deadline", "Countdown" };

    /* header */
    DrawRectangle((int)table.x, (int)table.y, (int)table.width, ROW_H, T.sidebar);
    for (int i = 0; i < COLS; i++)
        draw_text_fit(headers[i], colX[i] + 12, table.y + 11, colW[i] - 16, 18, WHITE);

    /* rows that match the search */
    int rows[MAX_ASSETS];
    int n = 0;
    for (int i = 0; i < store_count(); i++)
        if (matches_search(store_get(i))) rows[n++] = i;

    /* scrolling */
    Vector2 mouse = GetMousePosition();
    bool overBody = !confirmDelete && CheckCollisionPointRec(mouse, body);
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

        Urgency urg = store_urgency(a, nowT);
        if (a->id != selectedId) {            /* tint rows that need attention */
            if (urg == URGENCY_OVERDUE)   DrawRectangleRec(row, Fade(T.error, 0.14f));
            else if (urg == URGENCY_SOON) DrawRectangleRec(row, Fade(T.warn, 0.12f));
        }

        if (overBody && CheckCollisionPointRec(mouse, row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            selectedId = a->id;

        int ty = (int)y + 11;
        char due[16], cd[48];
        store_format_date(a->deadline, due, sizeof(due));
        store_countdown_text(a, nowT, cd, sizeof(cd));

        DrawText(TextFormat("%d", a->id), (int)colX[0] + 12, ty, 18, T.muted);
        draw_text_fit(a->name,       colX[1] + 12, (float)ty, colW[1] - 16, 18, T.text);
        draw_text_fit(a->category,   colX[2] + 12, (float)ty, colW[2] - 16, 18, T.text);
        draw_text_fit(a->assignedTo, colX[3] + 12, (float)ty, colW[3] - 16, 18, T.text);
        draw_text_fit(STATUS_NAME(a->status), colX[4] + 12, (float)ty, colW[4] - 16, 18, STATUS_COLORS[a->status]);
        draw_text_fit(due[0] ? due : "-", colX[5] + 12, (float)ty, colW[5] - 16, 18, T.text);
        draw_text_fit(cd, colX[6] + 12, (float)ty, colW[6] - 16, 18,
                      urg == URGENCY_NONE ? T.muted : urgency_color(urg));
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
    Rectangle card = { area.x, area.y + 60, cardW, 470 };
    DrawRectangleRounded(card, 0.04f, 8, T.card);

    float x = card.x + 24;
    float w = card.width - 48;
    float y = card.y + 24;

    struct { const char *label; char *buf; int size; } fields[] = {
        { "Name",        draft.name,       NAME_LEN  },
        { "Category",    draft.category,   SHORT_LEN },
        { "Assigned to", draft.assignedTo, SHORT_LEN },
        { "Deadline (YYYY-MM-DD, optional)", deadlineBuf, (int)sizeof(deadlineBuf) },
    };

    for (int i = 0; i < 4; i++) {
        DrawText(fields[i].label, (int)x, (int)y, 20, T.muted);
        Rectangle box = { x, y + 26, w, 36 };
        if (GuiTextBox(box, fields[i].buf, fields[i].size, activeField == i))
            activeField = (activeField == i) ? -1 : i;

        if (i == 3) {   /* live preview of what the typed date means */
            time_t t;
            if (deadlineBuf[0] == '\0') {
                if (activeField != 3)
                    DrawText("e.g. 2026-12-31", (int)box.x + 10, (int)box.y + 8, 20, T.muted);
            } else if (store_parse_date(deadlineBuf, &t)) {
                Asset tmp;
                memset(&tmp, 0, sizeof(tmp));
                tmp.deadline = t;
                char cd[48];
                store_countdown_text(&tmp, nowT, cd, sizeof(cd));
                Urgency u = store_urgency(&tmp, nowT);
                DrawText(cd, (int)(x + w - MeasureText(cd, 18)), (int)y + 2, 18, urgency_color(u));
            }
        }
        y += 74;
    }

    DrawText("Status", (int)x, (int)y, 20, T.muted);
    Rectangle statusBtn = { x, y + 26, w, 36 };
    if (GuiButton(statusBtn, TextFormat("%s   (click to change)", STATUS_NAME(draft.status))))
        draft.status = (AssetStatus)((draft.status + 1) % STATUS_COUNT);
    y += 74;

    if (formError[0] != '\0')
        DrawText(formError, (int)x, (int)y - 12, 20, T.error);

    Rectangle saveBtn   = { x, y + 8, 140, 40 };
    Rectangle cancelBtn = { x + 156, y + 8, 140, 40 };

    if (GuiButton(saveBtn, editing ? "Save changes" : "Save")) {
        time_t due = 0;
        if (draft.name[0] == '\0') {
            snprintf(formError, sizeof(formError), "Name is required.");
        } else if (deadlineBuf[0] != '\0' && !store_parse_date(deadlineBuf, &due)) {
            snprintf(formError, sizeof(formError), "Deadline must be a valid date (YYYY-MM-DD).");
        } else {
            draft.deadline = due;              /* 0 when the box was left empty */
            if (editing) {
                store_update(&draft);          /* draft.id is still the original id */
            } else {
                int newId = store_add(&draft);
                if (newId < 0) {
                    snprintf(formError, sizeof(formError), "Asset list is full (%d).", MAX_ASSETS);
                    return;
                }
                anim_queue_delivery(newId);    /* 3D view: a truck brings it in */
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

/* ---------- delete confirmation ----------
 * Shared by the 2D list and the 3D view (both use the same Delete button). While it is
 * open, GuiLock() + the confirmDelete checks above stop everything behind it from reacting. */
static void draw_delete_modal(void)
{
    Asset *a = store_find_by_id(confirmId);
    if (!a) { confirmDelete = false; return; }

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.55f));

    Rectangle dlg = { (GetScreenWidth() - 440) / 2.0f, (GetScreenHeight() - 210) / 2.0f, 440, 210 };
    DrawRectangleRounded(dlg, 0.06f, 8, T.card);
    DrawRectangleRoundedLines(dlg, 0.06f, 8, T.line);

    DrawText("Delete this asset?", (int)dlg.x + 24, (int)dlg.y + 22, 26, T.text);
    draw_text_fit(a->name, dlg.x + 24, dlg.y + 64, dlg.width - 48, 20, T.accent);
    DrawText("This cannot be undone.", (int)dlg.x + 24, (int)dlg.y + 94, 18, T.muted);

    Rectangle yesBtn = { dlg.x + 24,                  dlg.y + dlg.height - 64, 190, 40 };
    Rectangle noBtn  = { dlg.x + dlg.width - 24 - 190, dlg.y + dlg.height - 64, 190, 40 };

    if (GuiButton(yesBtn, "Yes, delete")) {
        anim_queue_removal(a);            /* 3D view: a worker carries the box out */
        store_remove_by_id(confirmId);
        if (selectedId == confirmId) selectedId = -1;
        persist();
        confirmDelete = false;
    } else if (GuiButton(noBtn, "No, cancel") || IsKeyPressed(KEY_ESCAPE)) {
        confirmDelete = false;            /* nothing is deleted */
    }
}

/* ---------- public API ---------- */
void ui_init(void)
{
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);   /* default font looks sharpest at multiples of 10 */
    apply_theme(false);
    store_init();                           /* loads the CSV, or seeds sample data on first run */
    strncpy(dbPathBuf, store_get_path(), PATH_LEN - 1);
    reset_draft();
    simT = (double)time(NULL);
    speedIdx = SPEED_REAL;
}

void ui_update_and_draw(void)
{
    simT += (double)GetFrameTime() * SPEEDS[speedIdx];   /* advance the (possibly simulated) clock */
    if (simT < 946684800.0)  simT = 946684800.0;         /* keep within 2000 .. 2100 */
    if (simT > 4102444800.0) simT = 4102444800.0;
    nowT = (time_t)simT;
    if (!(currentScreen == SCREEN_ASSETS && view3D)) anim_clear();   /* animations only play in the 3D view */
    ClearBackground(T.bg);
    if (confirmDelete) GuiLock();         /* buttons / text boxes ignore input while asking */

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

    bool fullscreen3D = (currentScreen == SCREEN_ASSETS && view3D && fullView3D);
    if (!fullscreen3D) {
        draw_overdue_toast(area);
        draw_sidebar();   /* drawn last so a screen switch never clicks through */
    }

    if (confirmDelete) {
        GuiUnlock();
        draw_delete_modal();
    }
}