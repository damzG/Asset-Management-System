
#ifndef STORE_H
#define STORE_H

#include "models.h"

/* In-memory data layer */

void store_init_placeholder(void);
int store_count(void);
Asset *store_get(int index);
int store_add(const Asset *asset);
void store_remove_by_id(int id);

#endif
