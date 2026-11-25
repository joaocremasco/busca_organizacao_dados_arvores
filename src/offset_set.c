#include "offset_set.h"

#include <stdlib.h>

static int compare_offsets(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

bool offset_set_add(OFFSET_SET *set, uint64_t offset) {
    uint64_t *grown;
    size_t capacity;
    if (set->count == set->capacity) {
        capacity = set->capacity == 0 ? 32 : set->capacity * 2;
        if (capacity < set->capacity) return false;
        grown = realloc(set->items, capacity * sizeof(*grown));
        if (grown == NULL) return false;
        set->items = grown;
        set->capacity = capacity;
    }
    set->items[set->count++] = offset;
    return true;
}

void offset_set_sort_unique(OFFSET_SET *set) {
    size_t i, unique;
    if (set == NULL || set->count == 0) return;
    qsort(set->items, set->count, sizeof(*set->items), compare_offsets);
    unique = 1;
    for (i = 1; i < set->count; i++) {
        if (set->items[i] != set->items[unique - 1])
            set->items[unique++] = set->items[i];
    }
    set->count = unique;
}

void offset_set_destroy(OFFSET_SET *set) {
    if (set == NULL) return;
    free(set->items);
    set->items = NULL;
    set->count = 0;
    set->capacity = 0;
}
