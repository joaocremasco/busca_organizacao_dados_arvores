#include "../include/abb.h"
#include "../include/tree_entry.h"

#include <stdlib.h>
#include <string.h>

typedef struct abb_node {
    TREE_ENTRY entry;
    struct abb_node *left, *right;
} ABB_NODE;

struct abb {
    ABB_NODE *root;
    size_t distinct;
    uint64_t comparisons;
};

static void destroy_node(ABB_NODE *node) {
    if (node == NULL) return;
    destroy_node(node->left);
    destroy_node(node->right);
    tree_entry_destroy(&node->entry);
    free(node);
}

ABB *abb_create(void) { return calloc(1, sizeof(ABB)); }

void abb_destroy(ABB **abb) {
    if (abb == NULL || *abb == NULL) return;
    destroy_node((*abb)->root);
    free(*abb);
    *abb = NULL;
}

static ABB_NODE *insert_node(ABB *abb, ABB_NODE *node, const char *key,
                             uint64_t offset, bool *ok, bool *inserted) {
    if (node == NULL) {
        ABB_NODE *created = calloc(1, sizeof(*created));
        if (created == NULL || !tree_entry_init(&created->entry, key, offset)) {
            free(created);
            *ok = false;
            return NULL;
        }
        *inserted = true;
        return created;
    }
    abb->comparisons++;
    int cmp = strcmp(key, node->entry.key);
    if (cmp == 0) {
        *ok = tree_entry_append(&node->entry, offset);
        return node;
    }
    if (cmp < 0) {
        ABB_NODE *child = insert_node(abb, node->left, key, offset, ok, inserted);
        if (*ok) node->left = child;
    } else {
        ABB_NODE *child = insert_node(abb, node->right, key, offset, ok, inserted);
        if (*ok) node->right = child;
    }
    return node;
}

bool abb_insert(ABB *abb, const char *key, uint64_t offset) {
    if (abb == NULL || key == NULL) return false;
    bool ok = true, inserted = false;
    ABB_NODE *root = insert_node(abb, abb->root, key, offset, &ok, &inserted);
    if (ok) {
        abb->root = root;
        if (inserted) abb->distinct++;
    }
    return ok;
}

bool abb_find(const ABB *abb, const char *key, TREE_OFFSETS *result) {
    if (result != NULL) { result->items = NULL; result->count = 0; }
    if (abb == NULL || key == NULL || result == NULL) return false;
    ABB_NODE *node = abb->root;
    while (node != NULL) {
        ((ABB *)abb)->comparisons++;
        int cmp = strcmp(key, node->entry.key);
        if (cmp == 0) {
            result->items = node->entry.offsets;
            result->count = node->entry.offset_count;
            return true;
        }
        node = cmp < 0 ? node->left : node->right;
    }
    return false;
}

static size_t height(const ABB_NODE *node) {
    if (node == NULL) return 0;
    size_t left = height(node->left), right = height(node->right);
    return 1 + (left > right ? left : right);
}

TREE_STATS abb_stats(const ABB *abb) {
    TREE_STATS stats = {0};
    if (abb != NULL) {
        stats.distinct = abb->distinct;
        stats.height = height(abb->root);
        stats.comparisons = abb->comparisons;
    }
    return stats;
}

static bool write_node(FILE *file, const ABB_NODE *node) {
    uint8_t present = node != NULL;
    if (fwrite(&present, sizeof(present), 1, file) != 1) return false;
    if (node == NULL) return true;
    return tree_entry_write(file, &node->entry) &&
           write_node(file, node->left) && write_node(file, node->right);
}

bool abb_write(FILE *file, const ABB *abb) {
    return file != NULL && abb != NULL && write_node(file, abb->root);
}

static ABB_NODE *read_node(FILE *file, size_t *count, bool *ok) {
    uint8_t present;
    if (fread(&present, sizeof(present), 1, file) != 1) { *ok = false; return NULL; }
    if (present == 0) return NULL;
    ABB_NODE *node = calloc(1, sizeof(*node));
    if (node == NULL || !tree_entry_read(file, &node->entry)) {
        free(node); *ok = false; return NULL;
    }
    node->left = read_node(file, count, ok);
    if (!*ok) { destroy_node(node); return NULL; }
    node->right = read_node(file, count, ok);
    if (!*ok) { destroy_node(node); return NULL; }
    (*count)++;
    return node;
}

ABB *abb_read(FILE *file) {
    if (file == NULL) return NULL;
    ABB *abb = abb_create();
    if (abb == NULL) return NULL;
    bool ok = true;
    abb->root = read_node(file, &abb->distinct, &ok);
    if (!ok) { abb_destroy(&abb); return NULL; }
    return abb;
}
