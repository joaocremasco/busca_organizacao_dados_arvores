#ifndef PROJETO2_ITEM_H
#define PROJETO2_ITEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char *key;
    uint64_t *offsets;
    size_t offset_count;
    size_t offset_capacity;
} ITEM;

bool item_init(ITEM *item, const char *key, uint64_t offset);
bool item_append(ITEM *item, uint64_t offset);
void item_destroy(ITEM *item);
bool item_write(FILE *file, const ITEM *item);
bool item_read(FILE *file, ITEM *item, uint64_t data_size,
               uint64_t *record_count);

#endif
