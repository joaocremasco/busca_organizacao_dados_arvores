#ifndef PROJETO2_RUBRO_NEGRA_H
#define PROJETO2_RUBRO_NEGRA_H
#include "tree.h"
typedef struct rb_tree RB_TREE;
RB_TREE *rb_create(void);
void rb_destroy(RB_TREE **tree);
bool rb_insert(RB_TREE *tree, const char *key, uint64_t offset);
bool rb_find(const RB_TREE *tree, const char *key, TREE_OFFSETS *result,
             uint64_t *comparisons);
TREE_STATS rb_stats(const RB_TREE *tree);
bool rb_write(FILE *file, const RB_TREE *tree);
RB_TREE *rb_read(FILE *file, uint64_t data_size, uint64_t *record_count);
#endif
