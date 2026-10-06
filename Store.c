//
// Created by User on 10/6/2026.
//
#include "store.h"
#include <string.h>

static int assets[MAX_ASSETS];
static int   count  = 0;
static int   nextId = 1;

int store_count(void) { return count; }

Asset *store_get(int index)
{
    if (index < 0 || index >= count) return NULL;
    return &assets[index];
}

int store_add(const Asset *asset)
{
    if (count >= MAX_ASSETS) return -1;
    assets[count] = *asset;
    assets[count].id = nextId++;
    return assets[count++].id;
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