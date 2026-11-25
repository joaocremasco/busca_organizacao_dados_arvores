#ifndef PROJETO2_TREE_ENTRY_H
#define PROJETO2_TREE_ENTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char *key;
    uint64_t *offsets;
    size_t offset_count;
    size_t offset_capacity;
} TREE_ENTRY;

bool tree_entry_init(TREE_ENTRY *entry, const char *key, uint64_t offset);
bool tree_entry_append(TREE_ENTRY *entry, uint64_t offset);
void tree_entry_destroy(TREE_ENTRY *entry);
bool tree_entry_write(FILE *file, const TREE_ENTRY *entry);
bool tree_entry_read(FILE *file, TREE_ENTRY *entry);

#endif
