#include "abb.h"
#include "item.h"

#include <stdlib.h>
#include <string.h>

typedef struct abb_node {
    ITEM entry;
    struct abb_node *left;
    struct abb_node *right;
} ABB_NODE;

struct abb {
    ABB_NODE *root;
    size_t distinct;
    uint64_t comparisons;
};

typedef struct {
    ABB_NODE *node;
    size_t depth;
} HEIGHT_ITEM;

typedef struct {
    ABB_NODE **slot;
    const char *lower;
    const char *upper;
} READ_SLOT;

static bool slot_push(READ_SLOT **items, size_t *count, size_t *capacity,
                      READ_SLOT value) {
    READ_SLOT *grown;
    size_t next;
    if (*count == *capacity) {
        next = *capacity == 0 ? 32 : *capacity * 2;
        grown = realloc(*items, next * sizeof(*grown));
        if (grown == NULL) return false;
        *items = grown;
        *capacity = next;
    }
    (*items)[(*count)++] = value;
    return true;
}

static bool node_push(ABB_NODE ***items, size_t *count, size_t *capacity,
                      ABB_NODE *value) {
    ABB_NODE **grown;
    size_t next;
    if (*count == *capacity) {
        next = *capacity == 0 ? 32 : *capacity * 2;
        grown = realloc(*items, next * sizeof(**items));
        if (grown == NULL) return false;
        *items = grown;
        *capacity = next;
    }
    (*items)[(*count)++] = value;
    return true;
}

static void destroy_nodes(ABB_NODE *root) {
    ABB_NODE *left;
    ABB_NODE *next;
    while (root != NULL) {
        if (root->left != NULL) {
            left = root->left;
            root->left = left->right;
            left->right = root;
            root = left;
        } else {
            next = root->right;
            item_destroy(&root->entry);
            free(root);
            root = next;
        }
    }
}

ABB *abb_create(void) {
    return calloc(1, sizeof(ABB));
}

void abb_destroy(ABB **abb) {
    if (abb == NULL || *abb == NULL) return;
    destroy_nodes((*abb)->root);
    free(*abb);
    *abb = NULL;
}

bool abb_insert(ABB *abb, const char *key, uint64_t offset) {
    ABB_NODE *parent = NULL;
    ABB_NODE *node;
    ABB_NODE *created;
    int comparison = 0;
    if (abb == NULL || key == NULL) return false;
    node = abb->root;
    while (node != NULL) {
        parent = node;
        abb->comparisons++;
        comparison = strcmp(key, node->entry.key);
        if (comparison == 0) return item_append(&node->entry, offset);
        node = comparison < 0 ? node->left : node->right;
    }
    created = calloc(1, sizeof(*created));
    if (created == NULL || !item_init(&created->entry, key, offset)) {
        free(created);
        return false;
    }
    if (parent == NULL) abb->root = created;
    else if (comparison < 0) parent->left = created;
    else parent->right = created;
    abb->distinct++;
    return true;
}

bool abb_find(const ABB *abb, const char *key, TREE_OFFSETS *result,
              uint64_t *comparisons) {
    ABB_NODE *node;
    if (result != NULL) {
        result->items = NULL;
        result->count = 0;
    }
    if (abb == NULL || key == NULL || result == NULL) return false;
    node = abb->root;
    while (node != NULL) {
        if (comparisons != NULL) (*comparisons)++;
        int comparison = strcmp(key, node->entry.key);
        if (comparison == 0) {
            result->items = node->entry.offsets;
            result->count = node->entry.offset_count;
            return true;
        }
        node = comparison < 0 ? node->left : node->right;
    }
    return false;
}

static size_t abb_height(const ABB_NODE *root) {
    HEIGHT_ITEM *items = NULL;
    size_t count = 0, capacity = 0, height = 0;
    if (root == NULL) return 0;
    capacity = 32;
    items = malloc(capacity * sizeof(*items));
    if (items == NULL) return 0;
    items[count++] = (HEIGHT_ITEM){(ABB_NODE *)root, 1};
    while (count != 0) {
        HEIGHT_ITEM current = items[--count];
        HEIGHT_ITEM *grown;
        if (current.depth > height) height = current.depth;
        if (current.node->left != NULL || current.node->right != NULL) {
            size_t needed = count + 2;
            if (needed > capacity) {
                capacity *= 2;
                grown = realloc(items, capacity * sizeof(*items));
                if (grown == NULL) {
                    free(items);
                    return 0;
                }
                items = grown;
            }
            if (current.node->left != NULL)
                items[count++] = (HEIGHT_ITEM){current.node->left, current.depth + 1};
            if (current.node->right != NULL)
                items[count++] = (HEIGHT_ITEM){current.node->right, current.depth + 1};
        }
    }
    free(items);
    return height;
}

TREE_STATS abb_stats(const ABB *abb) {
    TREE_STATS stats = {0};
    if (abb != NULL) {
        stats.distinct = abb->distinct;
        stats.height = abb_height(abb->root);
        stats.comparisons = abb->comparisons;
    }
    return stats;
}

static bool write_nodes(FILE *file, const ABB_NODE *root) {
    ABB_NODE **items = NULL;
    size_t count = 0, capacity = 0;
    bool ok = node_push(&items, &count, &capacity, (ABB_NODE *)root);
    while (ok && count != 0) {
        ABB_NODE *node = items[--count];
        uint8_t present = node == NULL ? 0 : 1;
        ok = fwrite(&present, sizeof(present), 1, file) == 1;
        if (ok && node != NULL) {
            ok = item_write(file, &node->entry) &&
                 node_push(&items, &count, &capacity, node->right) &&
                 node_push(&items, &count, &capacity, node->left);
        }
    }
    free(items);
    return ok;
}

bool abb_write(FILE *file, const ABB *abb) {
    return file != NULL && abb != NULL && write_nodes(file, abb->root);
}

static bool read_nodes(FILE *file, ABB *abb, uint64_t data_size,
                       uint64_t *record_count) {
    READ_SLOT *slots = NULL;
    size_t count = 0, capacity = 0;
    READ_SLOT root_slot = {&abb->root, NULL, NULL};
    bool ok = slot_push(&slots, &count, &capacity, root_slot);
    while (ok && count != 0) {
        READ_SLOT current = slots[--count];
        ABB_NODE **slot = current.slot;
        uint8_t present;
        if (fread(&present, sizeof(present), 1, file) != 1 || present > 1) {
            ok = false;
        } else if (present == 1) {
            *slot = calloc(1, sizeof(**slot));
            if (*slot == NULL || !item_read(file, &(*slot)->entry, data_size,
                                             record_count)) {
                if (*slot != NULL) item_destroy(&(*slot)->entry);
                free(*slot);
                *slot = NULL;
                ok = false;
            } else if ((current.lower != NULL &&
                        strcmp((*slot)->entry.key, current.lower) <= 0) ||
                       (current.upper != NULL &&
                        strcmp((*slot)->entry.key, current.upper) >= 0)) {
                ok = false;
            } else {
                abb->distinct++;
                READ_SLOT right = {&(*slot)->right, (*slot)->entry.key, current.upper};
                READ_SLOT left = {&(*slot)->left, current.lower, (*slot)->entry.key};
                ok = slot_push(&slots, &count, &capacity, right) &&
                     slot_push(&slots, &count, &capacity, left);
            }
        }
    }
    free(slots);
    return ok;
}

ABB *abb_read(FILE *file, uint64_t data_size, uint64_t *record_count) {
    ABB *abb;
    if (file == NULL) return NULL;
    abb = abb_create();
    if (abb == NULL) return NULL;
    if (!read_nodes(file, abb, data_size, record_count)) {
        abb_destroy(&abb);
        return NULL;
    }
    return abb;
}
