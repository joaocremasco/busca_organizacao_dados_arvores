#include "avl.h"
#include "item.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

typedef struct avl_node {
    ITEM entry;
    int32_t height;
    struct avl_node *left, *right;
} AVL_NODE;

struct avl {
    AVL_NODE *root;
    size_t distinct;
    uint64_t comparisons, rotations;
};

static int node_height(const AVL_NODE *node) { return node == NULL ? 0 : node->height; }
static int max_int(int a, int b) { return a > b ? a : b; }
static void update_height(AVL_NODE *node) {
    node->height = 1 + max_int(node_height(node->left), node_height(node->right));
}

static void destroy_node(AVL_NODE *node) {
    if (node == NULL) return;
    destroy_node(node->left);
    destroy_node(node->right);
    item_destroy(&node->entry);
    free(node);
}

AVL *avl_create(void) { return calloc(1, sizeof(AVL)); }

void avl_destroy(AVL **avl) {
    if (avl == NULL || *avl == NULL) return;
    destroy_node((*avl)->root);
    free(*avl);
    *avl = NULL;
}

static AVL_NODE *rotate_right(AVL *avl, AVL_NODE *a) {
    AVL_NODE *b = a->left;
    a->left = b->right;
    b->right = a;
    update_height(a);
    update_height(b);
    avl->rotations++;
    return b;
}

static AVL_NODE *rotate_left(AVL *avl, AVL_NODE *a) {
    AVL_NODE *b = a->right;
    a->right = b->left;
    b->left = a;
    update_height(a);
    update_height(b);
    avl->rotations++;
    return b;
}

static AVL_NODE *rebalance(AVL *avl, AVL_NODE *node) {
    update_height(node);
    int balance = node_height(node->left) - node_height(node->right);
    if (balance > 1) {
        if (node_height(node->left->left) < node_height(node->left->right))
            node->left = rotate_left(avl, node->left);
        return rotate_right(avl, node);
    }
    if (balance < -1) {
        if (node_height(node->right->right) < node_height(node->right->left))
            node->right = rotate_right(avl, node->right);
        return rotate_left(avl, node);
    }
    return node;
}

static AVL_NODE *insert_node(AVL *avl, AVL_NODE *node, const char *key,
                             uint64_t offset, bool *ok, bool *inserted) {
    if (node == NULL) {
        AVL_NODE *created = calloc(1, sizeof(*created));
        if (created == NULL || !item_init(&created->entry, key, offset)) {
            free(created);
            *ok = false;
            return NULL;
        }
        created->height = 1;
        *inserted = true;
        return created;
    }
    avl->comparisons++;
    int cmp = strcmp(key, node->entry.key);
    if (cmp == 0) {
        *ok = item_append(&node->entry, offset);
        return node;
    }
    if (cmp < 0) {
        AVL_NODE *child = insert_node(avl, node->left, key, offset, ok, inserted);
        if (!*ok) return node;
        node->left = child;
    } else {
        AVL_NODE *child = insert_node(avl, node->right, key, offset, ok, inserted);
        if (!*ok) return node;
        node->right = child;
    }
    return *inserted ? rebalance(avl, node) : node;
}

bool avl_insert(AVL *avl, const char *key, uint64_t offset) {
    if (avl == NULL || key == NULL) return false;
    bool ok = true, inserted = false;
    AVL_NODE *root = insert_node(avl, avl->root, key, offset, &ok, &inserted);
    if (ok) {
        avl->root = root;
        if (inserted) avl->distinct++;
    }
    return ok;
}

bool avl_find(const AVL *avl, const char *key, TREE_OFFSETS *result,
              uint64_t *comparisons) {
    if (result != NULL) {
        result->items = NULL;
        result->count = 0;
    }
    if (avl == NULL || key == NULL || result == NULL) return false;
    AVL_NODE *node = avl->root;
    while (node != NULL) {
        if (comparisons != NULL) (*comparisons)++;
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

TREE_STATS avl_stats(const AVL *avl) {
    TREE_STATS stats = {0};
    if (avl != NULL) {
        stats.distinct = avl->distinct;
        stats.height = (size_t)node_height(avl->root);
        stats.comparisons = avl->comparisons;
        stats.rotations = avl->rotations;
    }
    return stats;
}

static bool write_node(FILE *file, const AVL_NODE *node) {
    uint8_t present = node != NULL;
    if (fwrite(&present, sizeof(present), 1, file) != 1) return false;
    if (node == NULL) return true;
    return item_write(file, &node->entry) &&
        write_u32(file, (uint32_t)node->height) &&
        write_node(file, node->left) && write_node(file, node->right);
}

bool avl_write(FILE *file, const AVL *avl) { return file != NULL && avl != NULL && write_node(file, avl->root); }

static AVL_NODE *read_node(FILE *file, size_t *count, bool *ok,
                           const char *lower, const char *upper,
                           uint64_t data_size, uint64_t *record_count) {
    uint8_t present;
    if (fread(&present, sizeof(present), 1, file) != 1 || present > 1) {
        *ok = false;
        return NULL;
    }
    if (present == 0) return NULL;
    AVL_NODE *node = calloc(1, sizeof(*node));
    uint32_t stored_height;
    if (node == NULL || !item_read(file, &node->entry, data_size, record_count) ||
        !read_u32(file, &stored_height)) {
        if (node != NULL) item_destroy(&node->entry);
        free(node);
        *ok = false;
        return NULL;
    }
    node->height = (int32_t)stored_height;
    if ((lower != NULL && strcmp(node->entry.key, lower) <= 0) ||
        (upper != NULL && strcmp(node->entry.key, upper) >= 0)) {
        destroy_node(node);
        *ok = false;
        return NULL;
    }
    node->left = read_node(file, count, ok, lower, node->entry.key,
                           data_size, record_count);
    if (!*ok) {
        destroy_node(node);
        return NULL;
    }
    node->right = read_node(file, count, ok, node->entry.key, upper,
                            data_size, record_count);
    if (!*ok) {
        destroy_node(node);
        return NULL;
    }
    update_height(node);
    int balance = node_height(node->left) - node_height(node->right);
    if (node->height != (int32_t)stored_height || balance < -1 || balance > 1) {
        destroy_node(node);
        *ok = false;
        return NULL;
    }
    (*count)++;
    return node;
}

AVL *avl_read(FILE *file, uint64_t data_size, uint64_t *record_count) {
    if (file == NULL) return NULL;
    AVL *avl = avl_create();
    if (avl == NULL) return NULL;
    bool ok = true;
    avl->root = read_node(file, &avl->distinct, &ok, NULL, NULL,
                          data_size, record_count);
    if (!ok) {
        avl_destroy(&avl);
        return NULL;
    }
    return avl;
}
