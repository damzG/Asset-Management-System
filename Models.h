//
// Created by User on 10/6/2026.
//

#ifndef ASSET_MANAGEMENT_SYSTEM_MODELS_H
#define ASSET_MANAGEMENT_SYSTEM_MODELS_H

#include <time.h>

#define MAX_ASSETS 100
#define NAME_LEN   100
#define SHORT_LEN  50
#define PATH_LEN   260   /* file paths typed in Settings */

typedef enum {
    STATUS_IN_USE = 0,
    STATUS_IN_STORAGE,
    STATUS_REPAIR,
    STATUS_COUNT
} AssetStatus;

typedef struct {
    int id;
    char name[NAME_LEN];
    char category[SHORT_LEN];
    char assignedTo[SHORT_LEN];
    AssetStatus status;
    time_t deadline;      /* end of the due date (local time); 0 = no deadline */
} Asset;

#endif // ASSET_MANAGEMENT_SYSTEM_MODELS_H