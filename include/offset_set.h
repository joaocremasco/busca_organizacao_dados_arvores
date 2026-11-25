#ifndef PROJETO2_OFFSET_SET_H
#define PROJETO2_OFFSET_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t *items;
    size_t count;
    size_t capacity;
} OFFSET_SET;

bool offset_set_add(OFFSET_SET *set, uint64_t offset);
void offset_set_sort_unique(OFFSET_SET *set);
void offset_set_destroy(OFFSET_SET *set);

#endif
