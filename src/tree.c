#include "tree.h"
#include "abb.h"
#include "avl.h"
#include "rubro_negra.h"

#include <stdlib.h>

struct tree {
    TREE_KIND kind;
    union { ABB *abb; AVL *avl; RB_TREE *rb; } impl;
};

TREE *tree_create(TREE_KIND kind) {
    if (kind < TREE_ABB || kind > TREE_RB) return NULL;
    TREE *tree = calloc(1, sizeof(*tree));
    if (tree == NULL) return NULL;
    tree->kind = kind;
    if (kind == TREE_ABB) tree->impl.abb = abb_create();
    else if (kind == TREE_AVL) tree->impl.avl = avl_create();
    else tree->impl.rb = rb_create();
    if ((kind == TREE_ABB && tree->impl.abb == NULL) ||
        (kind == TREE_AVL && tree->impl.avl == NULL) ||
        (kind == TREE_RB && tree->impl.rb == NULL)) {
        free(tree);
        return NULL;
    }
    return tree;
}

void tree_destroy(TREE **tree) {
    if (tree == NULL || *tree == NULL) return;
    if ((*tree)->kind == TREE_ABB) abb_destroy(&(*tree)->impl.abb);
    else if ((*tree)->kind == TREE_AVL) avl_destroy(&(*tree)->impl.avl);
    else rb_destroy(&(*tree)->impl.rb);
    free(*tree);
    *tree = NULL;
}

bool tree_insert(TREE *tree, const char *key, uint64_t offset) {
    if (tree == NULL) return false;
    if (tree->kind == TREE_ABB) return abb_insert(tree->impl.abb, key, offset);
    if (tree->kind == TREE_AVL) return avl_insert(tree->impl.avl, key, offset);
    return rb_insert(tree->impl.rb, key, offset);
}

bool tree_find(const TREE *tree, const char *key, TREE_OFFSETS *result,
               uint64_t *comparisons) {
    if (tree == NULL) return false;
    if (tree->kind == TREE_ABB) return abb_find(tree->impl.abb, key, result, comparisons);
    if (tree->kind == TREE_AVL) return avl_find(tree->impl.avl, key, result, comparisons);
    return rb_find(tree->impl.rb, key, result, comparisons);
}

TREE_STATS tree_stats(const TREE *tree) {
    TREE_STATS stats = {0};
    if (tree == NULL) return stats;
    if (tree->kind == TREE_ABB) return abb_stats(tree->impl.abb);
    if (tree->kind == TREE_AVL) return avl_stats(tree->impl.avl);
    return rb_stats(tree->impl.rb);
}

bool tree_write(FILE *file, const TREE *tree) {
    if (file == NULL || tree == NULL) return false;
    if (tree->kind == TREE_ABB) return abb_write(file, tree->impl.abb);
    if (tree->kind == TREE_AVL) return avl_write(file, tree->impl.avl);
    return rb_write(file, tree->impl.rb);
}

TREE *tree_read(FILE *file, TREE_KIND kind, uint64_t data_size,
                uint64_t *record_count) {
    if (file == NULL) return NULL;
    if (kind < TREE_ABB || kind > TREE_RB || record_count == NULL) return NULL;
    TREE *tree = calloc(1, sizeof(*tree));
    if (tree == NULL) return NULL;
    tree->kind = kind;
    if (kind == TREE_ABB) tree->impl.abb = abb_read(file, data_size, record_count);
    else if (kind == TREE_AVL) tree->impl.avl = avl_read(file, data_size, record_count);
    else if (kind == TREE_RB) tree->impl.rb = rb_read(file, data_size, record_count);
    else {
        free(tree);
        return NULL;
    }
    if ((kind == TREE_ABB && tree->impl.abb == NULL) ||
        (kind == TREE_AVL && tree->impl.avl == NULL) ||
        (kind == TREE_RB && tree->impl.rb == NULL)) {
        free(tree);
        return NULL;
    }
    return tree;
}
