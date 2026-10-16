//
// Created by User on 10/6/2026.
//
#include "Store.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

static Asset assets[MAX_ASSETS];
static int   count  = 0;
static int   nextId = 1;
static char  dbPath[PATH_LEN] = DEFAULT_DB_PATH;

static const char *STATUS_NAMES[STATUS_COUNT] = { "In use", "In storage", "In repair" };

const char *store_status_name(AssetStatus s)
{
    return (s >= 0 && s < STATUS_COUNT) ? STATUS_NAMES[s] : "Unknown";
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

/* ---------- sample data ---------- */
static void seed(const char *name, const char *category, const char *who, AssetStatus status)
{
    Asset a;
    memset(&a, 0, sizeof(a));
    strncpy(a.name, name, NAME_LEN - 1);
    strncpy(a.category, category, SHORT_LEN - 1);
    strncpy(a.assignedTo, who, SHORT_LEN - 1);
    a.status = status;
    store_add(&a);
}

void store_init_placeholder(void)
{
    count = 0;
    nextId = 1;
    seed("Dell Latitude 5440 Laptop", "Laptop",    "Aoife Murphy", STATUS_IN_USE);
    seed("HP LaserJet Pro M404",      "Printer",   "Front Office", STATUS_IN_USE);
    seed("Samsung 27in Monitor",      "Monitor",   "",             STATUS_IN_STORAGE);
    seed("Cisco Catalyst Switch",     "Network",   "Server Room",  STATUS_IN_USE);
    seed("iPad Air (10.9in)",         "Tablet",    "Sean Kelly",   STATUS_REPAIR);
    seed("Logitech Webcam C920",      "Peripheral","",             STATUS_IN_STORAGE);
    seed("Office Desk (Standing)",    "Furniture", "Niamh Byrne",  STATUS_IN_USE);
    seed("Projector Epson EB-X49",    "AV",        "",             STATUS_IN_STORAGE);
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
                      const char *who, const char *status)
{
    fprintf(f, "%s,", id);
    write_field(f, name);   fputc(',', f);
    write_field(f, cat);    fputc(',', f);
    write_field(f, who);    fputc(',', f);
    fprintf(f, "%s\n", status);
}

bool store_save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "id,name,category,assignedTo,status\n");
    for (int i = 0; i < count; i++) {
        char id[16], status[16];
        snprintf(id, sizeof(id), "%d", assets[i].id);
        snprintf(status, sizeof(status), "%d", (int)assets[i].status);
        write_row(f, id, assets[i].name, assets[i].category, assets[i].assignedTo, status);
    }
    return fclose(f) == 0;
}

bool store_export_csv(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "ID,Name,Category,Assigned to,Status\n");
    for (int i = 0; i < count; i++) {
        char id[16];
        snprintf(id, sizeof(id), "%d", assets[i].id);
        write_row(f, id, assets[i].name, assets[i].category, assets[i].assignedTo,
                  store_status_name(assets[i].status));
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

        char *fields[5];
        if (parse_csv_line(line, fields, 5) < 5) continue;

        int id = atoi(fields[0]);
        if (id <= 0) continue;                 /* skips the header row and junk */
        if (count >= MAX_ASSETS) break;

        Asset a;
        memset(&a, 0, sizeof(a));
        a.id = id;
        strncpy(a.name,       fields[1], NAME_LEN - 1);
        strncpy(a.category,   fields[2], SHORT_LEN - 1);
        strncpy(a.assignedTo, fields[3], SHORT_LEN - 1);
        int st = atoi(fields[4]);
        a.status = (st >= 0 && st < STATUS_COUNT) ? (AssetStatus)st : STATUS_IN_STORAGE;

        assets[count++] = a;
        if (id >= nextId) nextId = id + 1;
    }
    fclose(f);
    return true;
}