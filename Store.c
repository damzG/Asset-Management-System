//
// Created by User on 10/6/2026.
//
#include "Store.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>

static Asset assets[MAX_ASSETS];
static int   count  = 0;
static int   nextId = 1;
static char  dbPath[PATH_LEN] = DEFAULT_DB_PATH;

static const char *STATUS_NAMES[STATUS_COUNT] = { "In use", "In storage", "In repair" };

const char *store_status_name(AssetStatus s)
{
    return (s >= 0 && s < STATUS_COUNT) ? STATUS_NAMES[s] : "Unknown";
}

static const char *CATEGORY_NAMES[CATEGORY_COUNT] = {
    "Computers", "Electronics", "Office Equipment", "Furniture", "Tools", "Vehicles", "Appliances", "Supplies", "Other"
};

const char *store_category_name(int index)
{
    return (index >= 0 && index < CATEGORY_COUNT) ? CATEGORY_NAMES[index] : "Other";
}

static bool same_text(const char *a, const char *b)     /* case-insensitive equality */
{
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

/* Older files used more specific names; map them onto the general categories. */
static const struct { const char *name; int index; } LEGACY_CATEGORIES[] = {
    { "Laptop", 0 }, { "Tablet", 0 }, { "Monitor", 0 },
    { "Network", 1 }, { "Peripheral", 1 }, { "AV", 1 },
    { "Printer", 2 }
};

static int category_lookup(const char *name)           /* -1 if the name is not on the list */
{
    for (int i = 0; i < CATEGORY_COUNT - 1; i++)
        if (same_text(name, CATEGORY_NAMES[i])) return i;
    for (size_t i = 0; i < sizeof(LEGACY_CATEGORIES) / sizeof(LEGACY_CATEGORIES[0]); i++)
        if (same_text(name, LEGACY_CATEGORIES[i].name)) return LEGACY_CATEGORIES[i].index;
    return -1;
}

int store_category_index(const char *name)
{
    int i = category_lookup(name);
    return (i >= 0) ? i : CATEGORY_COUNT - 1;          /* anything not on the list goes under "Other" */
}

/* ---------- basic access ---------- */
int store_count(void) { return count; }

Asset *store_get(int index)
{
    if (index < 0 || index >= count) return NULL;
    return &assets[index];
}

Asset *store_find_by_id(int id)
{
    for (int i = 0; i < count; i++)
        if (assets[i].id == id) return &assets[i];
    return NULL;
}

int store_add(const Asset *asset)
{
    if (count >= MAX_ASSETS) return -1;
    assets[count] = *asset;
    assets[count].id = nextId++;
    return assets[count++].id;
}

bool store_update(const Asset *asset)
{
    Asset *target = store_find_by_id(asset->id);
    if (!target) return false;
    *target = *asset;
    return true;
}

void store_remove_by_id(int id)
{
    for (int i = 0; i < count; i++) {
        if (assets[i].id == id) {
            memmove(&assets[i], &assets[i + 1], (size_t)(count - i - 1) * sizeof(Asset));
            count--;
            return;
        }
    }
}

/* ---------- deadline helpers ---------- */
bool store_parse_date(const char *s, time_t *out)
{
    int y, m, d, used = 0;
    if (sscanf(s, "%d-%d-%d%n", &y, &m, &d, &used) != 3) return false;
    while (s[used] == ' ') used++;
    if (s[used] != '\0') return false;
    if (y < 2000 || y > 2100) return false;

    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    t.tm_hour = 23; t.tm_min = 59; t.tm_sec = 59;   /* a deadline lasts until the end of the day */
    t.tm_isdst = -1;

    time_t r = mktime(&t);
    /* mktime silently rolls 2026-02-30 into March, so check nothing was changed */
    if (r == (time_t)-1 || t.tm_year != y - 1900 || t.tm_mon != m - 1 || t.tm_mday != d) return false;

    *out = r;
    return true;
}

void store_format_date(time_t t, char *buf, size_t n)
{
    if (t == 0) { if (n) buf[0] = '\0'; return; }
    struct tm *lt = localtime(&t);
    if (!lt || strftime(buf, n, "%Y-%m-%d", lt) == 0) { if (n) buf[0] = '\0'; }
}

Urgency store_urgency(const Asset *a, time_t now)
{
    if (a->deadline == 0) return URGENCY_NONE;
    double left = difftime(a->deadline, now);
    if (left < 0) return URGENCY_OVERDUE;
    if (left <= DEADLINE_SOON_DAYS * 86400.0) return URGENCY_SOON;
    return URGENCY_OK;
}

void store_countdown_text(const Asset *a, time_t now, char *buf, size_t n)
{
    if (a->deadline == 0) { snprintf(buf, n, "-"); return; }

    long secs = (long)difftime(a->deadline, now);
    bool over = secs < 0;
    if (over) secs = -secs;

    long d = secs / 86400, h = (secs % 86400) / 3600, m = (secs % 3600) / 60;
    char span[32];
    if (d > 0)      snprintf(span, sizeof(span), "%ldd %ldh", d, h);
    else if (h > 0) snprintf(span, sizeof(span), "%ldh %ldm", h, m);
    else            snprintf(span, sizeof(span), "%ldm", m);

    snprintf(buf, n, over ? "%s overdue" : "%s left", span);
}

/* ---------- sample data ---------- */
#define NO_DEADLINE 9999

/* End of the day that is `days` from today (negative = in the past). */
static time_t due_in_days(int days)
{
    if (days == NO_DEADLINE) return 0;
    time_t t = time(NULL) + (time_t)days * 86400;
    struct tm tmv = *localtime(&t);
    tmv.tm_hour = 23; tmv.tm_min = 59; tmv.tm_sec = 59;
    tmv.tm_isdst = -1;
    return mktime(&tmv);
}

static void seed(const char *name, const char *category, const char *who, AssetStatus status, int dueInDays)
{
    Asset a;
    memset(&a, 0, sizeof(a));
    strncpy(a.name, name, NAME_LEN - 1);
    strncpy(a.category, category, SHORT_LEN - 1);
    strncpy(a.assignedTo, who, SHORT_LEN - 1);
    a.status = status;
    a.deadline = due_in_days(dueInDays);
    store_add(&a);
}

void store_init_placeholder(void)
{
    count = 0;
    nextId = 1;
    seed("Dell Latitude 5440 Laptop", "Computers",        "Aoife Murphy", STATUS_IN_USE,     14);
    seed("HP LaserJet Pro M404",      "Office Equipment", "Front Office", STATUS_IN_USE,     -2);
    seed("Samsung 27in Monitor",      "Computers",        "",             STATUS_IN_STORAGE, NO_DEADLINE);
    seed("Cisco Catalyst Switch",     "Electronics",      "Server Room",  STATUS_IN_USE,      2);
    seed("iPad Air (10.9in)",         "Computers",        "Sean Kelly",   STATUS_REPAIR,     -5);
    seed("Logitech Webcam C920",      "Electronics",      "",             STATUS_IN_STORAGE, NO_DEADLINE);
    seed("Office Desk (Standing)",    "Furniture",        "Niamh Byrne",  STATUS_IN_USE,     30);
    seed("Projector Epson EB-X49",    "Electronics",      "",             STATUS_IN_STORAGE,  0);
    seed("Lenovo ThinkPad X1 Carbon", "Computers",        "Declan Walsh", STATUS_IN_USE,     -9);
    seed("Zebra ZD421 Label Printer", "Office Equipment", "Warehouse",    STATUS_REPAIR,      1);
    seed("Meeting Room TV 55in",      "Electronics",      "Boardroom",    STATUS_IN_USE,      7);
    seed("Cordless Drill Set",        "Tools",            "Maintenance",  STATUS_IN_USE,     12);
    seed("Delivery Van",              "Vehicles",         "Logistics",    STATUS_IN_USE,      5);
    seed("Staff Room Coffee Machine", "Appliances",       "Staff Room",   STATUS_IN_USE,     NO_DEADLINE);
    seed("A4 Paper (10 boxes)",       "Supplies",         "",             STATUS_IN_STORAGE, NO_DEADLINE);
}

void store_init(void)
{
    if (!store_load(dbPath)) {
        store_init_placeholder();
        store_save(dbPath);
    }
}

/* ---------- CSV helpers ---------- */
const char *store_get_path(void) { return dbPath; }

void store_set_path(const char *path)
{
    strncpy(dbPath, path, PATH_LEN - 1);
    dbPath[PATH_LEN - 1] = '\0';
}

/* Writes a field wrapped in quotes, doubling any quote inside it. */
static void write_field(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"') fputc('"', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

static void write_row(FILE *f, const char *id, const char *name, const char *cat,
                      const char *who, const char *status, const char *deadline)
{
    fprintf(f, "%s,", id);
    write_field(f, name);   fputc(',', f);
    write_field(f, cat);    fputc(',', f);
    write_field(f, who);    fputc(',', f);
    fprintf(f, "%s,%s\n", status, deadline);
}

bool store_save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "id,name,category,assignedTo,status,deadline\n");
    for (int i = 0; i < count; i++) {
        char id[16], status[16], due[16];
        snprintf(id, sizeof(id), "%d", assets[i].id);
        snprintf(status, sizeof(status), "%d", (int)assets[i].status);
        store_format_date(assets[i].deadline, due, sizeof(due));
        write_row(f, id, assets[i].name, assets[i].category, assets[i].assignedTo, status, due);
    }
    return fclose(f) == 0;
}

bool store_export_csv(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "ID,Name,Category,Assigned to,Status,Deadline\n");
    for (int i = 0; i < count; i++) {
        char id[16], due[16];
        snprintf(id, sizeof(id), "%d", assets[i].id);
        store_format_date(assets[i].deadline, due, sizeof(due));
        write_row(f, id, assets[i].name, assets[i].category, assets[i].assignedTo,
                  store_status_name(assets[i].status), due);
    }
    return fclose(f) == 0;
}

/* Splits one CSV line IN PLACE into fields (handles "quoted, fields" and ""). */
static int parse_csv_line(char *line, char *fields[], int maxFields)
{
    int n = 0;
    char *p = line;

    while (n < maxFields) {
        char *out = p;
        fields[n] = out;

        if (*p == '"') {
            p++;
            while (*p) {
                if (*p == '"') {
                    if (p[1] == '"') { *out++ = '"'; p += 2; }
                    else { p++; break; }
                } else {
                    *out++ = *p++;
                }
            }
        } else {
            while (*p && *p != ',') *out++ = *p++;
        }

        while (*p && *p != ',') p++;     /* skip stray characters after a closing quote */
        bool more = (*p == ',');
        *out = '\0';
        n++;
        if (!more) break;
        p++;
    }
    return n;
}

bool store_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return false;

    count = 0;
    nextId = 1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        char *fields[6];
        int nFields = parse_csv_line(line, fields, 6);
        if (nFields < 5) continue;             /* 5 columns = older file without deadlines */

        int id = atoi(fields[0]);
        if (id <= 0) continue;                 /* skips the header row and junk */
        if (count >= MAX_ASSETS) break;

        Asset a;
        memset(&a, 0, sizeof(a));
        a.id = id;
        strncpy(a.name,       fields[1], NAME_LEN - 1);
        int ci = category_lookup(fields[2]);      /* old names like "Laptop" become "Computers" */
        strncpy(a.category,   ci >= 0 ? CATEGORY_NAMES[ci] : fields[2], SHORT_LEN - 1);
        strncpy(a.assignedTo, fields[3], SHORT_LEN - 1);
        int st = atoi(fields[4]);
        a.status = (st >= 0 && st < STATUS_COUNT) ? (AssetStatus)st : STATUS_IN_STORAGE;

        time_t due;
        if (nFields >= 6 && fields[5][0] != '\0' && store_parse_date(fields[5], &due))
            a.deadline = due;

        assets[count++] = a;
        if (id >= nextId) nextId = id + 1;
    }
    fclose(f);
    return true;
}