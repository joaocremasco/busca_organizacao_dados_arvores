#ifndef PROJETO2_AVL_H
#define PROJETO2_AVL_H
#include "tree.h"
typedef struct avl AVL;
AVL *avl_create(void);
void avl_destroy(AVL **avl);
bool avl_insert(AVL *avl, const char *key, uint64_t offset);
bool avl_find(const AVL *avl, const char *key, TREE_OFFSETS *result);
TREE_STATS avl_stats(const AVL *avl);
bool avl_write(FILE *file, const AVL *avl);
AVL *avl_read(FILE *file);
#endif
