#ifndef PROJETO2_ABB_H
#define PROJETO2_ABB_H
#include "tree.h"
typedef struct abb ABB;
ABB *abb_create(void);
void abb_destroy(ABB **abb);
bool abb_insert(ABB *abb, const char *key, uint64_t offset);
bool abb_find(const ABB *abb, const char *key, TREE_OFFSETS *result,
              uint64_t *comparisons);
TREE_STATS abb_stats(const ABB *abb);
bool abb_write(FILE *file, const ABB *abb);
ABB *abb_read(FILE *file, uint64_t data_size, uint64_t *record_count);
#endif
