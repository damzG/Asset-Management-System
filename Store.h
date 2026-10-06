//
// Created by User on 10/6/2026.
//

#ifndef STORE_H
#define STORE_H

#include "models.h"

/* In-memory data layer. Later, replace store.c with a SQLite-backed
   version that keeps these same function names, and the UI won't change. */

void   store_init_placeholder(void);   /* fills a few sample rows */
int    store_count(void);
int *store_get(int index);           /* NULL if index is out of range */
int    store_add(const int *asset);  /* returns new id, or -1 if full */
void   store_remove_by_id(int id);

#endif
