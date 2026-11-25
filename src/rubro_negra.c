#include "rubro_negra.h"
#include "item.h"

#include <stdlib.h>
#include <string.h>

typedef struct rb_node {
    ITEM entry;
    bool red;
    struct rb_node *parent, *left, *right;
} RB_NODE;

struct rb_tree {
    RB_NODE *root;
    size_t distinct;
    uint64_t comparisons, rotations, recolors;
};

static bool is_red(const RB_NODE *node) { return node != NULL && node->red; }
static void set_color(RB_TREE *tree, RB_NODE *node, bool red) {
    if (node != NULL && node->red != red) {
        node->red = red;
        tree->recolors++;
    }
}

static void destroy_node(RB_NODE *node) {
    if (node == NULL) return;
    destroy_node(node->left);
    destroy_node(node->right);
    item_destroy(&node->entry);
    free(node);
}

RB_TREE *rb_create(void) { return calloc(1, sizeof(RB_TREE)); }

void rb_destroy(RB_TREE **tree) {
    if (tree == NULL || *tree == NULL) return;
    destroy_node((*tree)->root);
    free(*tree);
    *tree = NULL;
}

static void replace_parent_child(RB_TREE *tree, RB_NODE *old, RB_NODE *replacement) {
    if (old->parent == NULL) tree->root = replacement;
    else if (old->parent->left == old) old->parent->left = replacement;
    else old->parent->right = replacement;
    if (replacement != NULL) replacement->parent = old->parent;
}

static void rotate_left(RB_TREE *tree, RB_NODE *a) {
    RB_NODE *b = a->right;
    a->right = b->left;
    if (b->left != NULL) b->left->parent = a;
    replace_parent_child(tree, a, b);
    b->left = a;
    a->parent = b;
    tree->rotations++;
}

static void rotate_right(RB_TREE *tree, RB_NODE *a) {
    RB_NODE *b = a->left;
    a->left = b->right;
    if (b->right != NULL) b->right->parent = a;
    replace_parent_child(tree, a, b);
    b->right = a;
    a->parent = b;
    tree->rotations++;
}

static RB_NODE *repair_left(RB_TREE *tree, RB_NODE *node) {
    RB_NODE *parent = node->parent;
    RB_NODE *grandparent = parent->parent;
    RB_NODE *uncle = grandparent->right;
    if (is_red(uncle)) {
        set_color(tree, parent, false);
        set_color(tree, uncle, false);
        set_color(tree, grandparent, true);
        return grandparent;
    }
    if (node == parent->right) {
        node = parent;
        rotate_left(tree, node);
        parent = node->parent;
        grandparent = parent->parent;
    }
    set_color(tree, parent, false);
    set_color(tree, grandparent, true);
    rotate_right(tree, grandparent);
    return node;
}

static RB_NODE *repair_right(RB_TREE *tree, RB_NODE *node) {
    RB_NODE *parent = node->parent;
    RB_NODE *grandparent = parent->parent;
    RB_NODE *uncle = grandparent->left;
    if (is_red(uncle)) {
        set_color(tree, parent, false);
        set_color(tree, uncle, false);
        set_color(tree, grandparent, true);
        return grandparent;
    }
    if (node == parent->left) {
        node = parent;
        rotate_right(tree, node);
        parent = node->parent;
        grandparent = parent->parent;
    }
    set_color(tree, parent, false);
    set_color(tree, grandparent, true);
    rotate_left(tree, grandparent);
    return node;
}

static void repair_after_insert(RB_TREE *tree, RB_NODE *node) {
    while (node != tree->root && is_red(node->parent)) {
        if (node->parent == node->parent->parent->left)
            node = repair_left(tree, node);
        else
            node = repair_right(tree, node);
    }
    set_color(tree, tree->root, false);
}

static size_t rb_height(const RB_NODE *node) {
    size_t left, right;
    if (node == NULL) return 0;
    left = rb_height(node->left);
    right = rb_height(node->right);
    return 1 + (left > right ? left : right);
}

bool rb_insert(RB_TREE *tree, const char *key, uint64_t offset) {
    if (tree == NULL || key == NULL) return false;
    RB_NODE *parent = NULL, *node = tree->root;
    int last_comparison = 0;
    while (node != NULL) {
        parent = node;
        tree->comparisons++;
        last_comparison = strcmp(key, node->entry.key);
        if (last_comparison == 0) return item_append(&node->entry, offset);
        node = last_comparison < 0 ? node->left : node->right;
    }
    RB_NODE *created = calloc(1, sizeof(*created));
    if (created == NULL || !item_init(&created->entry, key, offset)) {
        free(created);
        return false;
    }
    created->red = true;
    created->parent = parent;
    if (parent == NULL) tree->root = created;
    else if (last_comparison < 0) parent->left = created;
    else parent->right = created;
    tree->distinct++;
    repair_after_insert(tree, created);
    return true;
}

bool rb_find(const RB_TREE *tree, const char *key, TREE_OFFSETS *result,
             uint64_t *comparisons) {
    if (result != NULL) {
        result->items = NULL;
        result->count = 0;
    }
    if (tree == NULL || key == NULL || result == NULL) return false;
    RB_NODE *node = tree->root;
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

TREE_STATS rb_stats(const RB_TREE *tree) {
    TREE_STATS stats = {0};
    if (tree != NULL) {
        stats.distinct = tree->distinct;
        stats.height = rb_height(tree->root);
        stats.comparisons = tree->comparisons;
        stats.rotations = tree->rotations;
        stats.recolors = tree->recolors;
    }
    return stats;
}

static bool write_node(FILE *file, const RB_NODE *node) {
    uint8_t present = node != NULL;
    if (fwrite(&present, sizeof(present), 1, file) != 1) return false;
    if (node == NULL) return true;
    uint8_t color = node->red ? 1 : 0;
    return item_write(file, &node->entry) &&
        fwrite(&color, sizeof(color), 1, file) == 1 &&
        write_node(file, node->left) && write_node(file, node->right);
}

bool rb_write(FILE *file, const RB_TREE *tree) { return file != NULL && tree != NULL && write_node(file, tree->root); }

static RB_NODE *read_node(FILE *file, RB_NODE *parent, size_t *count,
                          bool *ok, int *black_height, const char *lower,
                          const char *upper, uint64_t data_size,
                          uint64_t *record_count) {
    uint8_t present;
    if (fread(&present, sizeof(present), 1, file) != 1 || present > 1) {
        *ok = false;
        return NULL;
    }
    if (present == 0) {
        *black_height = 1;
        return NULL;
    }
    RB_NODE *node = calloc(1, sizeof(*node));
    uint8_t color;
    if (node == NULL || !item_read(file, &node->entry, data_size, record_count) ||
        fread(&color, sizeof(color), 1, file) != 1 || color > 1) {
        if (node != NULL) item_destroy(&node->entry);
        free(node);
        *ok = false;
        return NULL;
    }
    node->red = color != 0;
    node->parent = parent;
    if ((lower != NULL && strcmp(node->entry.key, lower) <= 0) ||
        (upper != NULL && strcmp(node->entry.key, upper) >= 0)) {
        destroy_node(node);
        *ok = false;
        return NULL;
    }
    int left_black = 0, right_black = 0;
    node->left = read_node(file, node, count, ok, &left_black, lower,
                           node->entry.key, data_size, record_count);
    if (!*ok) {
        destroy_node(node);
        return NULL;
    }
    node->right = read_node(file, node, count, ok, &right_black,
                            node->entry.key, upper, data_size, record_count);
    if (!*ok) {
        destroy_node(node);
        return NULL;
    }
    if (left_black != right_black || (node->red && (is_red(node->left) || is_red(node->right)))) {
        destroy_node(node);
        *ok = false;
        return NULL;
    }
    *black_height = left_black + (node->red ? 0 : 1);
    (*count)++;
    return node;
}

RB_TREE *rb_read(FILE *file, uint64_t data_size, uint64_t *record_count) {
    if (file == NULL) return NULL;
    RB_TREE *tree = rb_create();
    if (tree == NULL) return NULL;
    bool ok = true;
    int black_height = 0;
    tree->root = read_node(file, NULL, &tree->distinct, &ok, &black_height,
                           NULL, NULL, data_size, record_count);
    if (!ok || is_red(tree->root)) {
        rb_destroy(&tree);
        return NULL;
    }
    return tree;
}
