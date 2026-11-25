#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_COUNT 700

static unsigned int next_random(unsigned int *state) {
    *state = *state * 1103515245U + 12345U;
    return *state;
}

static bool files_equal(FILE *left, FILE *right) {
    int a, b;
    rewind(left);
    rewind(right);
    do {
        a = fgetc(left);
        b = fgetc(right);
        if (a != b) return false;
    } while (a != EOF);
    return true;
}

static bool test_tree(TREE_KIND kind) {
    TREE *tree = tree_create(kind);
    TREE *copy = NULL;
    FILE *first = tmpfile();
    FILE *second = tmpfile();
    unsigned int order[KEY_COUNT];
    unsigned int state = 271828U;
    size_t i;
    uint64_t records = 0;
    bool ok = tree != NULL && first != NULL && second != NULL;
    if (!ok) goto finish;
    for (i = 0; i < KEY_COUNT; i++) order[i] = (unsigned int)i;
    for (i = KEY_COUNT - 1; i > 0; i--) {
        size_t j = next_random(&state) % (i + 1);
        unsigned int temp = order[i];
        order[i] = order[j];
        order[j] = temp;
    }
    for (i = 0; i < KEY_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%05u", order[i]);
        ok = tree_insert(tree, key, order[i] * 2) &&
             tree_insert(tree, key, order[i] * 2 + 1);
        if (!ok) goto finish;
    }
    ok = tree_stats(tree).distinct == KEY_COUNT && tree_write(first, tree);
    if (!ok) goto finish;
    rewind(first);
    copy = tree_read(first, kind, UINT64_MAX, &records);
    if (copy == NULL || records != KEY_COUNT * 2) {
        ok = false;
        goto finish;
    }
    ok = tree_write(second, copy) && files_equal(first, second);
    if (!ok) goto finish;
    for (i = 0; i < KEY_COUNT; i++) {
        char key[32];
        TREE_OFFSETS found;
        snprintf(key, sizeof(key), "key%05u", (unsigned int)i);
        if (!tree_find(copy, key, &found, NULL) || found.count != 2 ||
            found.items[0] != i * 2 || found.items[1] != i * 2 + 1) {
            ok = false;
            break;
        }
    }
finish:
    tree_destroy(&tree);
    tree_destroy(&copy);
    if (first != NULL) fclose(first);
    if (second != NULL) fclose(second);
    return ok;
}

int main(void) {
    TREE_KIND kinds[] = {TREE_ABB, TREE_AVL, TREE_RB};
    size_t i;
    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        if (!test_tree(kinds[i])) {
            fprintf(stderr, "roundtrip falhou para árvore %u\n", (unsigned int)kinds[i]);
            return 1;
        }
    }
    puts("roundtrip: OK");
    return 0;
}
