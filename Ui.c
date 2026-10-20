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

    if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
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
            store_format_date(a->deadline, deadlineBuf, sizeof(deadlineBuf));
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
    simT = (double)time(NULL);
    speedIdx = SPEED_REAL;
}

void ui_update_and_draw(void)
{
    simT += (double)GetFrameTime() * SPEEDS[speedIdx];   /* advance the (possibly simulated) clock */
    if (simT < 946684800.0)  simT = 946684800.0;         /* keep within 2000 .. 2100 */
    if (simT > 4102444800.0) simT = 4102444800.0;
    nowT = (time_t)simT;
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

    draw_overdue_toast(area);
    draw_sidebar();   /* drawn last so a screen switch never clicks through */
}