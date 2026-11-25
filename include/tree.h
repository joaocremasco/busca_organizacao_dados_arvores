#ifndef PROJETO2_TREE_H
#define PROJETO2_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum { TREE_ABB = 1, TREE_AVL = 2, TREE_RB = 3 } TREE_KIND;
typedef struct tree TREE;

typedef struct {
    size_t distinct;
    size_t height;
    uint64_t comparisons;
    uint64_t rotations;
    uint64_t recolors;
} TREE_STATS;

typedef struct {
    const uint64_t *items;
    size_t count;
} TREE_OFFSETS;

TREE *tree_create(TREE_KIND kind);
void tree_destroy(TREE **tree);
bool tree_insert(TREE *tree, const char *key, uint64_t offset);
bool tree_find(const TREE *tree, const char *key, TREE_OFFSETS *result,
               uint64_t *comparisons);
TREE_STATS tree_stats(const TREE *tree);
bool tree_write(FILE *file, const TREE *tree);
TREE *tree_read(FILE *file, TREE_KIND kind, uint64_t data_size,
                uint64_t *record_count);

#endif
