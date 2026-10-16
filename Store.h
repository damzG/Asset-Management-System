#ifndef STORE_H
#define STORE_H

#include <stdbool.h>
#include "Models.h"

#define DEFAULT_DB_PATH "assets.csv"

/* ---- In-memory data layer ---- */
void        store_init(void);              /* load DEFAULT_DB_PATH, or seed sample data if missing */
void        store_init_placeholder(void);  /* wipe and fill with sample data */
int         store_count(void);
Asset      *store_get(int index);
Asset      *store_find_by_id(int id);
int         store_add(const Asset *asset);   /* returns new id, or -1 if full */
bool        store_update(const Asset *asset);/* replaces the asset with the same id */
void        store_remove_by_id(int id);

/* ---- Persistence (CSV) ---- */
const char *store_get_path(void);
void        store_set_path(const char *path);
bool        store_load(const char *path);    /* replaces memory contents; false if unreadable */
bool        store_save(const char *path);    /* writes id,name,category,assignedTo,status */
bool        store_export_csv(const char *path); /* human-readable report (status as text) */

const char *store_status_name(AssetStatus s);

#endif