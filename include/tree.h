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
bool tree_find(const TREE *tree, const char *key, TREE_OFFSETS *result);
size_t tree_distinct_keys(const TREE *tree);
size_t tree_height(const TREE *tree);
uint64_t tree_comparisons(const TREE *tree);
uint64_t tree_rotations(const TREE *tree);
uint64_t tree_recolors(const TREE *tree);
TREE_STATS tree_stats(const TREE *tree);
bool tree_write(FILE *file, const TREE *tree);
TREE *tree_read(FILE *file, TREE_KIND kind);

#endif
